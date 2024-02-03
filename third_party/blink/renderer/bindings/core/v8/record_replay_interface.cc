// Copyright 2021 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/record_replay_interface.h"

#include "v8/third_party/inspector_protocol/crdtp/json.h"
#include "v8/third_party/inspector_protocol/crdtp/serializable.h"

#include "third_party/inspector_protocol/crdtp/json.h"
#include "third_party/inspector_protocol/crdtp/serializable.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/process/process_handle.h"
#include "base/record_replay.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/events/custom_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_debugger_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_resource_container.h"
#include "third_party/blink/renderer/core/inspector/inspector_resource_content_loader.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/inspector/resolve_node.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/inspector_protocol/crdtp/maybe.h"
#include "v8/include/v8-inspector.h"

#include <array>
#include <fstream>
#include <string>
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

#ifndef OS_WIN
static const char DirectorySeparator = '/';
#else
static const char DirectorySeparator = '\\';
#endif

static const char *AnnotationHookJSName = "__RECORD_REPLAY_ANNOTATION_HOOK__";

namespace v8 {

extern void FunctionCallbackRecordReplaySetCommandCallback(const FunctionCallbackInfo<Value>& args);
extern void FunctionCallbackRecordReplaySetClearPauseDataCallback(const FunctionCallbackInfo<Value>& callArgs);
extern void FunctionCallbackRecordReplayAddNewScriptHandler(const FunctionCallbackInfo<Value>& args);
extern void FunctionCallbackRecordReplayGetScriptSource(const FunctionCallbackInfo<Value>& args);

namespace internal {

extern int RecordReplayObjectId(v8::Isolate* isolate, v8::Local<v8::Context> cx,
                                v8::Local<v8::Value> object, bool allow_create);
extern void RecordReplayConfirmObjectHasId(v8::Isolate* isolate,
                                           v8::Local<v8::Context> cx,
                                           v8::Local<v8::Value> object);
extern v8::Local<v8::Object> RecordReplayGetBytecode(
    v8::Isolate* isolate_,
    v8::Local<v8::Object> paramsObj);

} // namespace internal
} // namespace v8

#define CDPERROR_MISSINGCONTEXT 1001
#define CDPERROR_NOTALIVE 1002

namespace blink {
// using RemoteObjectIdTypeRaw = v8_inspector::String16;
// The actual type for RemoteObjectId
using RemoteObjectIdTypeRaw = std::u16string;

// The more convenient type that we use
using RemoteObjectIdType = WTF::String;

extern "C" void V8RecordReplaySetDefaultContext(v8::Isolate* isolate, v8::Local<v8::Context> cx);
extern "C" void V8RecordReplayFinishRecording();
extern "C" void V8RecordReplaySetCrashReason(const char* reason);
extern "C" char* V8RecordReplayReadAssetFileContents(const char* aPath, size_t* aLength);

static const char REPLAY_CDT_PAUSE_OBJECT_GROUP[] =
    "REPLAY_CDT_PAUSE_OBJECT_GROUP";

static bool IsGReplayScriptEnabledWhenRecording() {
  return !recordreplay::FeatureEnabled("replay-only-command-handling");
}

static bool IsGReplayScriptEnabled() {
  return recordreplay::IsReplaying() ||
         IsGReplayScriptEnabledWhenRecording();
}

static LocalFrame* GetLocalFrameRoot(v8::Isolate* isolate) {
  LocalDOMWindow* currentWindow = CurrentDOMWindow(isolate);

  if (!currentWindow) {
    recordreplay::Print("[RuntimeError] GetLocalFrameRoot: no window.");
    return nullptr;
  }

  LocalFrame *f = currentWindow->GetFrame();
  if (!f || f->IsDetached() || f->IsProvisional()) {
    recordreplay::Print("[RuntimeError] GetLocalFrameRoot: window has no frame.");
    return nullptr;
  }

  LocalFrame& root = f->LocalFrameRoot();

  if (root.IsDetached() || root.IsProvisional()) {
    recordreplay::Print("[RuntimeError] GetLocalFrameRoot: root is detached or provisional.");
    return nullptr;
  }

  return &root;
}


class InspectorData {
public:
  // These are untraced, because they are rooted in a global variable which
  // won't be crawled by the GC.  However, I don't think we need to worry about
  // this, as inspector actions can't be initiated against non-existant frames,
  // and likewise, any inspector objects that get culled should never be accessible
  // via an inspector action (any inspector action will be against some other 
  // isolate/frame/context group that exists), so we shouldn't be able to cause an 
  // invalid dereference.
  v8::Isolate* isolate;
  UntracedMember<InspectorDOMAgent> inspectorDomAgent;
  UntracedMember<InspectorDOMDebuggerAgent> inspectorDomDebuggerAgent;
  UntracedMember<InspectorNetworkAgent> inspectorNetworkAgent;
  UntracedMember<InspectorCSSAgent> inspectorCssAgent;
  UntracedMember<InspectedFrames> inspectedFrames;
  v8_inspector::V8InspectorSession* inspectorSession;

  InspectorData(v8::Isolate* i) {
    isolate = i; 
    inspectorDomAgent = nullptr;
    inspectorDomDebuggerAgent = nullptr;
    inspectorNetworkAgent = nullptr;
    inspectorCssAgent = nullptr;
    inspectedFrames = nullptr;
    inspectorSession = nullptr;
  }

  LocalFrame* GetLocalFrameRoot() const { return blink::GetLocalFrameRoot(isolate); }
};

static LocalFrame* gRootLocalFrame = nullptr;

typedef std::unordered_map<int, InspectorData*> ContextGroupIdInspectorMap;

std::unordered_map<v8::Isolate*, ContextGroupIdInspectorMap*>* gInspectorData = nullptr;
std::unordered_map<v8::Isolate*, v8_inspector::V8Inspector*>* gV8Inspectors = nullptr;

static std::string ReadReplayAssetFileRaw(const char* filename, size_t& len) {
  const char* scriptDir = getenv("RECORD_REPLAY_ASSETS_DIRECTORY");
  if (!scriptDir) {
    recordreplay::Crash("ReadReplayAssetFileRaw failed: RECORD_REPLAY_ASSETS_DIRECTORY not provided");
  }

  std::string fpath = std::string(scriptDir) + std::string("/") + filename;
  std::ifstream ifs(fpath);
  std::stringstream ss;
  ss << ifs.rdbuf();
  std::string s = ss.str();
  len = s.length();
  if (!len) {
    recordreplay::Crash("ReadReplayAssetFileRaw failed: %s", fpath.c_str());
  }
  return s;
}

static String ReadReplayAssetFile(const char* fname) {
  // "__RECORD_REPLAY_ARGUMENTS__.log(`DDBG ReplayCommandHandler ############`)";
  size_t len;

  // Important: Treat as UTF-8.
  String result = String::FromUTF8(
    IsGReplayScriptEnabledWhenRecording()
      // Recording + Replay.
      ? ReadReplayAssetFileRaw(fname, len).c_str()
      // Replay only.
      : V8RecordReplayReadAssetFileContents(fname, &len),
    len
  );
  if (!len) {
    recordreplay::Crash("ReadReplayAssetFile failed: %s", fname);
  }
  recordreplay::Print("DDBG ReadReplayAssetFile %zu", len);
  return result;
}

// static
String ReadReplayCommandHandlerScript() {
  return ReadReplayAssetFile("replay_command_handlers.js");
}


/** ###########################################################################
 * gSourceMapScript
 * ##########################################################################*/

// Script which sets a handler for collecting source maps from scripts in the
// recording. Runs when recording/replaying if source map collection is enabled.
const char* gSourceMapScript = R""""(
//js
(() => {

const {
  log,
  warning,
  getRecordingId,
  sha256DigestHex,
  writeToRecordingDirectory,
  addRecordingEvent,
  addNewScriptHandler,
  getScriptSource,
  recordingDirectoryFileExists,
  readFromRecordingDirectory,
  getRecordingFilePath,
  RECORD_REPLAY_DISABLE_SOURCEMAP_CACHE,
} = __RECORD_REPLAY_ARGUMENTS__;

const cache = {};

// Provide a cache for urls, salted with the supplied hash.  Practically, this
// means if the script content changes at the url, we will re-download the resource.
async function getCachedResource(url, hash) {
  const key = `${url}:${hash}`;
  if (cache[key] && !RECORD_REPLAY_DISABLE_SOURCEMAP_CACHE) {
    return cache[key];
  }

  log(`fetching sourcemap resource ${key}`);

  const res = await fetchText(url);
  cache[key] = res;
  return res;
}

addNewScriptHandler(async (scriptId, sourceURL, relativeSourceMapURL) => {
  try {
  if (!relativeSourceMapURL || relativeSourceMapURL.startsWith("data:"))
    return;

  const recordingId = getRecordingId();
  if (!recordingId) {
    // The recording has been invalidated.
    return;
  }

  const urls = getSourceMapURLs(sourceURL, relativeSourceMapURL);
  if (!urls)
    return;

  const scriptSource = getScriptSource(scriptId);
  const scriptHash = sha256DigestHex(scriptSource);

  const { sourceMapURL, sourceMapBaseURL } = urls;

  let sourceMap;
  try {
    sourceMap = await getCachedResource(sourceMapURL, scriptHash);
  } catch (err) {
    log(`[RuntimeError] Failed to read sourcemap ${sourceMapURL}: ${err.message}`);
  }
  if (!sourceMap) {
    return;
  }

  const id = scriptHash;
  const name = `sourcemap-${id}.map`;
  const lookupName = `sourcemap-${id}.lookup`;

  let sources;
  if (recordingDirectoryFileExists(name) && recordingDirectoryFileExists(lookupName)) {
    try {
      sources = JSON.parse(readFromRecordingDirectory(lookupName));
    } catch (err) {
      log(`[RuntimeError][sourcemaps] Failed to load sourcemaps from file: ${lookupName} - ${err.message}`);
    }
  }

  if (!sources) {
    writeToRecordingDirectory(name, sourceMap);

    sources = collectUnresolvedSourceMapResources(sourceMap, sourceMapURL, sourceURL);
    writeToRecordingDirectory(lookupName, JSON.stringify(sources));
  }

  addRecordingEvent(JSON.stringify({
    kind: "sourcemapAdded",
    path: getRecordingFilePath(name),
    recordingId,
    id,
    url: sourceMapURL,
    baseURL: sourceMapBaseURL,
    targetContentHash: `sha256:${scriptHash}`,
    targetURLHash: sourceURL ? makeAPIHash(sourceURL) : undefined,
    targetMapURLHash: makeAPIHash(sourceMapURL),
  }));

  for (const { offset, url } of sources) {
    let sourceContent;
    try {
      sourceContent = await getCachedResource(url, scriptHash);
    } catch (err) {
      log(`[RuntimeError] Failed to read original source ${url}: ${err.message}`);
      continue;
    }
    const hash = sha256DigestHex(sourceContent);
    const name = `source-${hash}`;

    if (!recordingDirectoryFileExists(name)) {
      writeToRecordingDirectory(name, sourceContent);
    }
    addRecordingEvent(JSON.stringify({
      kind: "originalSourceAdded",
      path: getRecordingFilePath(name),
      recordingId,
      parentId: id,
      parentOffset: offset,
    }));
  }
  } catch (err) {
    warning(`[sourcemaps] Error: ${err?.stack || err}`);
  }
});

async function fetchText(url) {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Fetching ${url} failed with status code ${response.status} (${response.statusText})`);
  }
  return await response.text();
}

function makeAPIHash(content) {
  assert(typeof content === "string");
  const digestHex = sha256DigestHex(content);
  return "sha256:" + digestHex;
}

function collectUnresolvedSourceMapResources(mapText, mapURL) {
  let obj;
  try {
    obj = JSON.parse(mapText);
    if (typeof obj !== "object" || !obj) {
      return {
        sources: [],
      };
    }
  } catch (err) {
    log(`Exception parsing sourcemap JSON (${mapURL})`);
    return {
      sources: [],
    };
  }

  const unresolvedSources = [];
  let sourceOffset = 0;

  function logError(msg) {
    log(`[RuntimeError][sourcemaps] ${msg} (${mapURL}:${sourceOffset})`);
  }

  if (obj.version !== 3) {
    logError("Invalid sourcemap version");
    return {
      sources: [],
    };
  }

  if (obj.sources != null) {
    const { sourceRoot, sources, sourcesContent } = obj;

    if (Array.isArray(sources)) {
      for (let i = 0; i < sources.length; i++) {
        const offset = sourceOffset++;

        if (
          !Array.isArray(sourcesContent) ||
          typeof sourcesContent[i] !== "string"
        ) {
          let url = sources[i];
          if (typeof sourceRoot === "string" && sourceRoot) {
            url = sourceRoot.replace(/\/?/, "/") + url;
          }
          let sourceURL;
          try {
            sourceURL = new URL(url, mapURL).toString();
          } catch {
            logError("Unable to compute original source URL: " + url);
            continue;
          }

          unresolvedSources.push({
            offset,
            url: sourceURL,
          });
        }
      }
    } else {
      logError("Invalid sourcemap source list");
    }
  }

  return unresolvedSources;
}

function assert(v, msg = "") {
  if (!v) {
    const m = `Assertion failed when handling command (${msg})`;
    log(`[RuntimeError] ${m} - ${Error().stack}`);
    throw new Error(m);
  }
}

function getSourceMapURLs(sourceURL, relativeSourceMapURL) {
  let sourceBaseURL;
  if (typeof sourceURL === "string" && isValidBaseURL(sourceURL)) {
    sourceBaseURL = sourceURL;
  } else if (window?.location?.href && isValidBaseURL(window?.location?.href)) {
    sourceBaseURL = window.location.href;
  }

  let sourceMapURL;
  try {
    sourceMapURL = new URL(relativeSourceMapURL, sourceBaseURL).toString();
  } catch (err) {
    log("Failed to process sourcemap url: " + err.message);
    return null;
  }

  // If the map was a data: URL or something along those lines, we want
  // to resolve paths in the map relative to the overall base.
  const sourceMapBaseURL =
    isValidBaseURL(sourceMapURL) ? sourceMapURL : sourceBaseURL;

  return { sourceMapURL, sourceMapBaseURL };
}

function isValidBaseURL(url) {
  try {
    new URL("", url);
    return true;
  } catch {
    return false;
  }
}

})();

)"""";

// Script that injects React DevTools "stub" functions to capture
// marker annotations while recording, for use in later processing
const char* gReactDevtoolsScript = R""""(
//js
(() => {

const stubFiberRoots = {};
const unmountedFibersByRenderer = {};
const unmountedFiberAlternatesByRenderer = {};

const stubHook = {
  isStub: true,
  supportsFiber: true,
  inject,
  onCommitFiberUnmount,
  onCommitFiberRoot,
  onPostCommitFiberRoot,
  renderers: new Map(),
};


function getFiberRootsSetForRenderer(rendererID) {
  if (!stubFiberRoots[rendererID]) {
    stubFiberRoots[rendererID] = new Set();
  }

  return stubFiberRoots[rendererID];
}

function getUnmountedFibersSetForRenderer(rendererID) {
  if (!unmountedFibersByRenderer[rendererID]) {
    unmountedFibersByRenderer[rendererID] = new Set();
  }

  return unmountedFibersByRenderer[rendererID];
}

function getUnmountedFiberAlternatesForRenderer(rendererID) {
  if (!unmountedFiberAlternatesByRenderer[rendererID]) {
    unmountedFiberAlternatesByRenderer[rendererID] = new Map();
  }

  return unmountedFiberAlternatesByRenderer[rendererID];
}

window.__REACT_DEVTOOLS_SAVED_RENDERERS__ = [];
window.__REACT_DEVTOOLS_STUB_FIBER_ROOTS = stubFiberRoots;

Object.defineProperty(window, "__REACT_DEVTOOLS_GLOBAL_HOOK__", {
  configurable: true,
  enumerable: false,
  get() {
    return stubHook;
  }
});

let uidCounter = 0;

function inject(renderer) {
  // Declare these enum strings in scope for later routine use
  const annotationType = "inject";

  const id = ++uidCounter;
  window.__RECORD_REPLAY_ANNOTATION_HOOK__("react-devtools-hook:v1:" + annotationType, "");
  window.__REACT_DEVTOOLS_SAVED_RENDERERS__.push(renderer);
  return id;
}

function onCommitFiberUnmount(rendererID, fiber) {
  const annotationType = "commit-fiber-unmount"

  // Unmounts are always one fiber at a time during the commit phase.
  // Stash the unmounted fibers here, so we can map them to persistent
  // object IDs inside of `onCommitFiberRoot` processing in the routine.
  const unmountedFibersSet = getUnmountedFibersSetForRenderer(rendererID);
  unmountedFibersSet.add(fiber);

  let unmountedFiberAlternates;
  if (fiber.alternate) {
    unmountedFiberAlternates = getUnmountedFiberAlternatesForRenderer(rendererID);
    unmountedFiberAlternates.set(fiber, fiber.alternate);
  }

  window.__RECORD_REPLAY_ANNOTATION_HOOK__("react-devtools-hook:v1:" + annotationType, "");
}

// eslint-disable-next-line no-unused-vars
function onCommitFiberRoot(rendererID, root, priorityLevel) {
  // The "commit" handler should be the only one the routine needs to do the work as of 2023-05-01.
  // We capture unmounted fibers in the unmount handler above, and the routine
  // will process them when we evaluate at the commit annotation point.
  // The others mostly exist for hypothetical completeness.
  const annotationType = "commit-fiber-root";

  const mountedRoots = getFiberRootsSetForRenderer(rendererID);
  const current = root.current;
  const isKnownRoot = mountedRoots.has(root);
  // Keep track of mounted roots so we can hydrate when DevTools connect.
  const isUnmounting = current.memoizedState == null || current.memoizedState.element == null;

  if (!isKnownRoot && !isUnmounting) {
    mountedRoots.add(root);
  } else if (isKnownRoot && isUnmounting) {
    mountedRoots.delete(root);
  }

  // Get these so it's in scope in the routine eval, and we can clear it after the annotation
  const unmountedFibersSet = getUnmountedFibersSetForRenderer(rendererID);
  const unmountedFiberAlternates = getUnmountedFiberAlternatesForRenderer(rendererID);

  window.__RECORD_REPLAY_ANNOTATION_HOOK__("react-devtools-hook:v1:" + annotationType, "");

  for (const fiber of unmountedFibersSet) {
    unmountedFiberAlternates.delete(fiber);
  }
  unmountedFibersSet.clear();
}

// eslint-disable-next-line no-unused-vars
function onPostCommitFiberRoot(rendererID, root) {
  const annotationType = "post-commit-fiber-root";
  window.__RECORD_REPLAY_ANNOTATION_HOOK__("react-devtools-hook:v1:" + annotationType, "");
}

})();

)"""";



// Script that injects Redux DevTools "stub" functions to capture
// marker annotations while recording, for use in later processing
const char* gReduxDevtoolsScript = R""""(
//js
(() => { // webpackBootstrap
/******/ 	"use strict";
var __webpack_exports__ = {};

;// CONCATENATED MODULE: ./src/pageScript/api/generateInstanceId.ts
let id = 0;
function generateId(instanceId) {
  return instanceId || ++id;
}
;// CONCATENATED MODULE: ./src/pageScript/api/filters.ts
const FilterState = {
  DO_NOT_FILTER: 'DO_NOT_FILTER',
  DENYLIST_SPECIFIC: 'DENYLIST_SPECIFIC',
  ALLOWLIST_SPECIFIC: 'ALLOWLIST_SPECIFIC'
};
const noFiltersApplied = localFilter => !localFilter && (!window.devToolsOptions || !window.devToolsOptions.filter || window.devToolsOptions.filter === FilterState.DO_NOT_FILTER);
function isFiltered(action, localFilter) {
  if (noFiltersApplied(localFilter) || typeof action !== 'string' && typeof action.type.match !== 'function') {
    return false;
  }
  const {
    allowlist,
    denylist
  } = localFilter || window.devToolsOptions || {};
  const actionType = action.type || action;
  return allowlist && !actionType.match(allowlist) || denylist && actionType.match(denylist);
}
;// CONCATENATED MODULE: ./src/pageScript/api/index.ts


const listeners = {};
function isArray(arg) {
  return Array.isArray(arg);
}
function getLocalFilter(config) {
  const denylist = config.actionsDenylist ?? config.actionsBlacklist;
  const allowlist = config.actionsAllowlist ?? config.actionsWhitelist;
  if (denylist || allowlist) {
    return {
      allowlist: isArray(allowlist) ? allowlist.join('|') : allowlist,
      denylist: isArray(denylist) ? denylist.join('|') : denylist
    };
  }
  return undefined;
}
let latestDispatchedActions = {};
function saveReplayAnnotation(action, state, connectionType, extractedConfig, config) {
  const {
    instanceId
  } = extractedConfig;
  window.__RECORD_REPLAY_ANNOTATION_HOOK__('redux-devtools-setup', JSON.stringify({
    type: 'action',
    actionType: action.type,
    connectionType,
    instanceId
  }));
  latestDispatchedActions[instanceId] = {
    action,
    state,
    extractedConfig,
    config
  };
}
function sendMessage(action, state, preConfig = {}, _instanceId, _name) {
  if (!action || !action.type) {
    action = {
      type: 'update'
    };
  } else if (typeof action === 'string') {
    action = {
      type: action
    };
  }
  const [config, extractedExtensionConfig] = extractExtensionConfig(preConfig);
  saveReplayAnnotation(action, state, 'generic', extractedExtensionConfig, config);
}
function extractExtensionConfig(preConfig) {
  const config = preConfig || {};
  const instanceId = generateId(config.instanceId);
  if (!config.instanceId) config.instanceId = instanceId;
  if (!config.name) {
    config.name = document.title && instanceId === 1 ? document.title : `Instance ${instanceId}`;
  }
  const localFilter = getLocalFilter(config);
  let {
    stateSanitizer,
    actionSanitizer,
    predicate
  } = config;
  const extractedExtensionConfig = {
    instanceId: instanceId,
    stateSanitizer,
    actionSanitizer,
    predicate,
    localFilter,
    isFiltered: isFiltered
  };
  return [config, extractedExtensionConfig];
}
function connect(preConfig) {
  const [config, extractedExtensionConfig] = extractExtensionConfig(preConfig);
  const {
    instanceId
  } = extractedExtensionConfig;
  const subscribe = listener => {
    if (!listener) return undefined;
    return function unsubscribe() {};
  };
  const unsubscribe = () => {
    delete listeners[instanceId];
  };
  const send = (action, state) => {
    if (!action) {
      return;
    }
    let amendedAction = action;
    if (typeof action === 'string') {
      amendedAction = {
        type: action
      };
    }
    saveReplayAnnotation(amendedAction, state, 'generic', extractedExtensionConfig, config);
    return;
  };
  const init = (_state, _liftedData) => {
    window.__RECORD_REPLAY_ANNOTATION_HOOK__('redux-devtools-setup', JSON.stringify({
      type: 'init',
      connectionType: 'generic',
      instanceId
    }));
  };
  const error = (_payload) => {};
  return {
    init,
    subscribe,
    unsubscribe,
    send,
    error
  };
}
;// CONCATENATED MODULE: ./src/pageScript/index.ts


let stores = {};
function __REDUX_DEVTOOLS_EXTENSION__(preConfig = {}) {
  // if (typeof config !== 'object') config = {};
  if (!window.devToolsOptions) window.devToolsOptions = {};
  let store;
  const [config, extractedExtensionConfig] = extractExtensionConfig(preConfig);
  const {
    instanceId
  } = extractedExtensionConfig;
  function init() {
    window.__RECORD_REPLAY_ANNOTATION_HOOK__('redux-devtools-setup', JSON.stringify({
      type: 'init',
      connectionType: 'redux',
      instanceId
    }));
  }
  const enhance = () => next => {
    return (reducer_, initialState_) => {
      const originalStore = next(reducer_, initialState_);
      const newStore = {
        ...originalStore,
        dispatch: action => {
          const result = originalStore.dispatch(action);
          saveReplayAnnotation(action, originalStore.getState(), 'redux', extractedExtensionConfig, config);
          return result;
        }
      };

      // @ts-ignore
      store = stores[instanceId] = newStore;
      init();
      return store;
    };
  };
  return enhance();
}
// noinspection JSAnnotator
window.__REDUX_DEVTOOLS_EXTENSION__ = __REDUX_DEVTOOLS_EXTENSION__;
window.__REDUX_DEVTOOLS_EXTENSION__.open = () => {};
window.__REDUX_DEVTOOLS_EXTENSION__.notifyErrors = () => {};
window.__REDUX_DEVTOOLS_EXTENSION__.send = sendMessage;
window.__REDUX_DEVTOOLS_EXTENSION__.listen = () => {};
window.__REDUX_DEVTOOLS_EXTENSION__.connect = connect;
window.__REDUX_DEVTOOLS_EXTENSION__.disconnect = () => {};
const extensionCompose = config => (...funcs) => {
  // @ts-ignore FIXME
  return (...args) => {
    const instanceId = generateId(config.instanceId);
    return [...funcs].reduceRight(
    // @ts-ignore FIXME
    (composed, f) => f(composed), __REDUX_DEVTOOLS_EXTENSION__({
      ...config,
      instanceId
    })(...args));
  };
};
function reduxDevtoolsExtensionCompose(...funcs) {
  if (funcs.length === 0) {
    return __REDUX_DEVTOOLS_EXTENSION__();
  }
  if (funcs.length === 1 && typeof funcs[0] === 'object') {
    return extensionCompose(funcs[0]);
  }
  return extensionCompose({})(...funcs);
}
window.__REDUX_DEVTOOLS_EXTENSION_COMPOSE__ = reduxDevtoolsExtensionCompose;
/******/ })()

)"""";

const char* gOnNewWindowScript = R""""(
//js
(() => {
  window.__REACT_DEVTOOLS_GLOBAL_HOOK__ = window.top.__REACT_DEVTOOLS_GLOBAL_HOOK__;
  window.__REDUX_DEVTOOLS_EXTENSION__ = window.top.__REDUX_DEVTOOLS_EXTENSION__;
  window.__REDUX_DEVTOOLS_EXTENSION_COMPOSE__ = window.top.__REDUX_DEVTOOLS_EXTENSION_COMPOSE__;

  // TODO: Feels like this cross-context function usage can cause trouble, especially when
  //      the user pauses inside the iframe's JS and tries to access something inside the iframe via 
  //      __RECORD_REPLAY__?
  window.__RECORD_REPLAY__ = window.top.__RECORD_REPLAY__;
  window.__RECORD_REPLAY_ARGUMENTS__ = window.top.__RECORD_REPLAY_ARGUMENTS__;
})()
)"""";

static v8::Local<v8::String> ToV8String(v8::Isolate* isolate, const char* value) {
  return v8::String::NewFromUtf8(isolate, value,
                                 v8::NewStringType::kInternalized).ToLocalChecked();
}

// Define a property that isn't writable, configurable, or enumerable.
static void DefineProperty(v8::Isolate* isolate, v8::Local<v8::Object> obj,
                           const char* name, v8::Local<v8::Value> value) {
  v8::Local<v8::String> name_string = ToV8String(isolate, name);
  obj->DefineOwnProperty(isolate->GetCurrentContext(), name_string, value,
                         (v8::PropertyAttribute)(v8::ReadOnly | v8::DontEnum | v8::DontDelete))
    .Check();
}

static void SetFunctionProperty(v8::Isolate* isolate, v8::Local<v8::Object> obj,
                                const char* name, v8::FunctionCallback callback) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::FunctionTemplate> function_template =
    v8::FunctionTemplate::New(isolate, callback, v8::Local<v8::Value>(),
                              v8::Local<v8::Signature>(), 0,
                              v8::ConstructorBehavior::kThrow,
                              v8::SideEffectType::kHasSideEffect);
  v8::Local<v8::Function> function =
    function_template->GetFunction(context).ToLocalChecked();

  DefineProperty(isolate, obj, name, function);
  function->SetName(ToV8String(isolate, name));
}

static void LogCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() &&
        "must be called with a single string");
  v8::String::Utf8Value text(args.GetIsolate(), args[0]);
  recordreplay::Print("%s", *text);
}

static void LogTraceCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() &&
        "must be called with a single string");
  v8::String::Utf8Value text(args.GetIsolate(), args[0]);
  recordreplay::Trace("%s", *text);
}

static void LogWarningCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() &&
        "must be called with a single string");
  v8::String::Utf8Value text(args.GetIsolate(), args[0]);
  recordreplay::Warning("%s", *text);
}

void
RecordReplayRegisterV8Inspector(v8_inspector::V8Inspector* inspector,
                                v8::Isolate* isolate) {
  if (v8::IsMainThread() && IsGReplayScriptEnabled()) {
    if (!gV8Inspectors) {
      gV8Inspectors = new std::unordered_map<v8::Isolate*,v8_inspector::V8Inspector*>();
      gInspectorData = new std::unordered_map<v8::Isolate*, ContextGroupIdInspectorMap*>();
    }

    gV8Inspectors->insert(std::make_pair(isolate, inspector));
  }
}

// Whether the frame that our globally registered script(s)
// were run in is alive.
static bool gReplayScriptsAlive = false;

/**
 * This is called when our local root frame is about to shut down.
 */
void RecordReplayClearContexts(const char* reason, LocalFrame* frame) {
  CHECK(v8::IsMainThread());
  if (!gReplayScriptsAlive || frame != gRootLocalFrame) {
    return;
  }
  recordreplay::Print("ReplayScript STATUS_CHANGE_UNALIVE - %s", reason);
  gReplayScriptsAlive = false;
}

static void fromJsIsReplayScriptAlive(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  args.GetReturnValue().Set(v8::Number::New(isolate, gReplayScriptsAlive));
}

// Function to invoke on CDP responses and events.
static v8::Eternal<v8::Function>* gCDPMessageCallback;

static void SetCDPMessageCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  CHECK(args[0]->IsFunction());
  v8::Local<v8::Function> callback = args[0].As<v8::Function>();
  gCDPMessageCallback = new v8::Eternal<v8::Function>(isolate, callback);
}

static void SendMessageToFrontend(const v8_inspector::StringView& message) {
  recordreplay::AutoDisallowEvents disallow(
      "RecordReplay_SendMessageToFrontend");
  CHECK(v8::IsMainThread());

  CHECK(gCDPMessageCallback);

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (!isolate->InContext() || ScriptForbiddenScope::IsScriptForbidden()) {
    // We're never interested in messages sent at these times.
    return;
  }

  v8::HandleScope scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> arg;
  if (message.is8Bit()) {
    arg = v8::String::NewFromOneByte(isolate, message.characters8(),
                                                        v8::NewStringType::kNormal,
                                                        (int)message.length()).ToLocalChecked();
  } else {
    arg = v8::String::NewFromTwoByte(isolate, message.characters16(),
                                                        v8::NewStringType::kNormal,
                                                        (int)message.length()).ToLocalChecked();
  }
  v8::Local<v8::Function> callback = gCDPMessageCallback->Get(isolate);
  v8::MaybeLocal<v8::Value> rv = callback->Call(context, v8::Undefined(isolate), 1, &arg);
  CHECK(!rv.IsEmpty());

  // If we get back a string from the call, report it as an error to the log (in such a way as it
  // can be recovered by our error reporting), and then crash.
  v8::Local<v8::Value> result = rv.ToLocalChecked();
  CHECK(result->IsUndefined() || result->IsString());

  if (result->IsString()) {
    v8::String::Utf8Value messageValue(isolate, result);
    std::string messageStr(*messageValue);
    recordreplay::Crash("CDPMessageCallback FAILED %s:%d %s", "js", 0, messageStr.c_str());
  }
}

struct InspectorChannel final : public v8_inspector::V8Inspector::Channel {
  void sendResponse(int callId,
                    std::unique_ptr<v8_inspector::StringBuffer> message) final {
    SendMessageToFrontend(message->string());
  }
  void sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message) final {
    SendMessageToFrontend(message->string());
  }
  void flushProtocolNotifications() final {}
};

absl::optional<int> GetCurrentContextGroupIdForIsolate(v8::Isolate* isolate) {
  LocalFrame* local_frame_root = GetLocalFrameRoot(isolate);

  if (local_frame_root != nullptr) {
    // Get (do NOT create) a ContextGroupId:
    return WeakIdentifierMap<LocalFrame>::Identifier(local_frame_root);
  }

  return absl::optional<int>();
}

InspectorData* getInspectorFor(v8::Isolate* isolate, int contextGroupId) {
  InspectorData* data = nullptr;
  ContextGroupIdInspectorMap* inspectorData;

  CHECK(gInspectorData);

  if (gInspectorData->find(isolate) == gInspectorData->end()) {
    inspectorData = new ContextGroupIdInspectorMap();
    gInspectorData->insert(std::make_pair(isolate, inspectorData));
  } else {
    inspectorData = (*gInspectorData)[isolate];
  }

  if (inspectorData->find(contextGroupId) == inspectorData->end()) {
    data = new InspectorData(isolate);
    inspectorData->insert(std::pair(contextGroupId, data));
  } else {
    data = inspectorData->at(contextGroupId);
  }
  return data;
}

/**
 * This function makes sure that the session exists and
 * we are on main thread when accessing it.
 */
v8_inspector::V8InspectorSession* getInspectorSession(v8::Isolate* isolate, int currentContextId) {
  CHECK(v8::IsMainThread());
  CHECK(IsGReplayScriptEnabled());
  CHECK(gV8Inspectors);

  v8_inspector::V8Inspector* inspector = (*gV8Inspectors)[isolate];
  CHECK(inspector);

  InspectorData* data = getInspectorFor(isolate, currentContextId);

  if (!data->inspectorSession) {
    recordreplay::AutoDisallowEvents disallow("RecordReplayRegisterV8Inspector");
    data->inspectorSession = inspector->connect(currentContextId,
                                            new InspectorChannel(),
                                            v8_inspector::StringView(),
                                            v8_inspector::V8Inspector::kFullyTrusted).release();
  }
  return data->inspectorSession;
}

static int GetBlinkPersistentId(v8::Local<v8::Object> object) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  // Provide a unique id for Blink Objects.
  if (V8DOMWrapper::IsWrapper(isolate, object)) {
    ScriptWrappable* wrappable = ToScriptWrappable(object);
    return wrappable->RecordReplayId();
  }

  return 0;
}

// Get persistent id of objects that we are currently tracking.
static int GetPersistentId(v8::Local<v8::Object> object) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  return v8::internal::RecordReplayObjectId(isolate,
                                            isolate->GetCurrentContext(),
                                            object,
                                            /* allow_create */ false);
}

/**
 * This only supports V8 CDP commands.
 * That is because we do not have access to a complete DevToolsSession
 * (A fully connected session in turn would use the UberDispatcher to distribute
 * arbitrary commands to all parts of Chromium.)
 * That is because a full session (i) might add a lot of overhead, and/or
 * (ii) cause many more types of divergences.
 * That is why we would need to create individual Inspectors/agents (e.g. InspectorDOMAgent) as needed instead.
 * However, we might also opt to forego that option entirely, and do it all manually, since many inspectors/agents
 * do not provide too much value if they are not hooked up to a `DevToolsSession` and the `UberDispatcher`.
 */
static void SendCDPMessage(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() &&
        "must be called with a single string");

  v8::Isolate* isolate = args.GetIsolate();
  absl::optional<int> contextGroupId = GetCurrentContextGroupIdForIsolate(isolate);

  // It can be the case that we simply don't have a context group id (a local frame) at this
  // time; just log it and inform the client.
  if (!contextGroupId.has_value()) {
    if (gCDPMessageCallback != nullptr) {
      // Ensure the message has an ID. If not, handle the error in JavaScript.
      v8::String::Utf8Value inmessage(args.GetIsolate(), args[0]);
      std::string nmessage(*inmessage);
      absl::optional<base::Value> jsonMessage = base::JSONReader::Read(nmessage);
      base::Value::Dict* messageDict = jsonMessage->GetIfDict();
      CHECK(messageDict != nullptr);
      CHECK(messageDict->FindInt("id").has_value());

      // Construct our error result.
      std::unique_ptr<base::DictionaryValue> error(new base::DictionaryValue);
      error->SetStringKey("message", "[RUN-2600] No context group available for Isolate.");
      error->SetIntKey("code", CDPERROR_MISSINGCONTEXT);

      base::DictionaryValue result;
      result.SetKey("error", base::Value::FromUniquePtrValue(std::move(error)));
      result.SetIntKey("id", *(messageDict->FindInt("id")));

      std::string json;
      base::JSONWriter::Write(result, &json);
      auto message = v8_inspector::StringView((const uint8_t*)json.c_str(), json.length());
      SendMessageToFrontend(message);
    }
    return;
  }

  v8::String::Utf8Value message(args.GetIsolate(), args[0]);

  std::string nmessage(*message);
  v8_inspector::StringView messageView((const uint8_t*)nmessage.c_str(), nmessage.length());
  getInspectorSession(isolate, *contextGroupId)->dispatchProtocolMessage(messageView);
}

static std::string GetRecordingDirectory() {
  const char* recordingDir = getenv("RECORD_REPLAY_DIRECTORY");
  if (recordingDir) {
    return recordingDir;
  }
  const char* homeDir = getenv("HOME");
  if (!homeDir) {
    homeDir = getenv("USERPROFILE");
  }
  return std::string(homeDir) + DirectorySeparator + std::string(".replay");
}

static void GetRecordingId(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  const char* recordingId = recordreplay::GetRecordingId();
  if (recordingId) {
    args.GetReturnValue().Set(ToV8String(isolate, recordingId));
  } else {
    args.GetReturnValue().SetNull();
  }
}

static void SHA256DigestHex(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() &&
      "must be called with a single string");
  v8::Isolate* isolate = args.GetIsolate();
  v8::String::Utf8Value content(isolate, args[0]);

  std::unique_ptr<crypto::SecureHash> hasher =
    crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  hasher->Update(*content, content.length());
  uint8_t digest[crypto::kSHA256Length];
  hasher->Finish(digest, crypto::kSHA256Length);
  char* digestHex = new char[65];
  for (int i = 0; i < 32; i++) {
    sprintf(digestHex + i * 2, "%02x", digest[i]);
  }

  args.GetReturnValue().Set(ToV8String(isolate, digestHex));
}

static void GetRecordingFilePath(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() && "must be called with one string");
  v8::Isolate* isolate = args.GetIsolate();
  v8::String::Utf8Value filename(isolate, args[0]);

  std::string path = GetRecordingDirectory() + DirectorySeparator + std::string(*filename);

  args.GetReturnValue().Set(ToV8String(isolate, path.c_str()));
}

static void RecordingDirectoryFileExists(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() && "must be called with one string");
  v8::Isolate* isolate = args.GetIsolate();
  v8::String::Utf8Value filename(isolate, args[0]);

  std::string path = GetRecordingDirectory() + DirectorySeparator + std::string(*filename);

  std::ifstream stream(path);

  args.GetReturnValue().Set(v8::Boolean::New(isolate, stream.good()));
}

static void ReadFromRecordingDirectory(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() && "must be called with one string");
  v8::Isolate* isolate = args.GetIsolate();
  v8::String::Utf8Value filename(isolate, args[0]);

  std::string path = GetRecordingDirectory() + DirectorySeparator + std::string(*filename);
  std::ifstream stream(path);
  std::string data;
  stream >> data;
  stream.close();

  args.GetReturnValue().Set(ToV8String(isolate, data.c_str()));
}

static void WriteToRecordingDirectory(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 2 && args[0]->IsString() && args[1]->IsString() &&
        "must be called with two strings");
  v8::Isolate* isolate = args.GetIsolate();
  v8::String::Utf8Value filename(isolate, args[0]);
  v8::String::Utf8Value content(isolate, args[1]);

  recordreplay::Assert("[RUN-1670-1764] WriteToRecordingDirectory %s (%zu)", *filename, (size_t)strlen(*content));

  std::string path = GetRecordingDirectory() + DirectorySeparator + std::string(*filename);
  std::ofstream stream(path);
  stream << *content;
  stream.close();

  args.GetReturnValue().Set(ToV8String(isolate, path.c_str()));
}

static void AddRecordingEvent(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() &&
        "must be called with a single string");
  v8::String::Utf8Value content(args.GetIsolate(), args[0]);

  std::string filename = GetRecordingDirectory() + DirectorySeparator + std::string("recordings.log");
  std::ofstream stream(filename.c_str(), std::ofstream::app);
  stream << *content << "\n";
  stream.close();
}

static void fromJsGetPersistentId(const v8::FunctionCallbackInfo<v8::Value>& args) {
  int persistentId = 0;
  v8::Isolate* isolate = args.GetIsolate();
  if (args.Length() == 1 && args[0]->IsObject()) {
    persistentId = GetPersistentId(args[0].As<v8::Object>());
  }
  args.GetReturnValue().Set(v8::Number::New(isolate, persistentId));
}

static void fromJsCheckPersistentId(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() >= 1) {
    v8::internal::RecordReplayConfirmObjectHasId(args.GetIsolate(),
                                                 args.GetIsolate()->GetCurrentContext(),
                                                 args[0]);
  }
}

static void GetCurrentError(const v8::FunctionCallbackInfo<v8::Value>& args);


static void SetDataProperty(v8::Isolate* isolate,
                            v8::Local<v8::Object> obj,
                            const char* property,
                            v8::Local<v8::Value> value) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  obj->Set(context, ToV8String(isolate, property), value).Check();
}

/** ###########################################################################
 * CBOR stuff
 * ##########################################################################*/

/**
 * NOTE: There are two identical `Serializable` interfaces -
 *  `v8_crdtp::Serializable` and
 *  `crdtp::Serializable`.
 *  Both namespaces also have their own copy of the ConvertCBORToJSON function.
 *
 * @see https://replit.com/@Domiii/FunctionTemplates#main.cpp
 */
// typedef ConvertResult (*ConvertFun)(int, int);  // signature for all valid functions
template <typename S,
          std::string Convert(const std::vector<uint8_t>&,
                              std::vector<uint8_t>&)>
v8::MaybeLocal<v8::Value> convertCborToJSTempl(v8::Isolate* isolate,
                                               S* value) {
  // deserialize + send to JS
  std::vector<uint8_t> cbor;
  value->AppendSerialized(&cbor);

  if (cbor.size() > 1) {
    /**
     * This is based on other code that uses `wrapObject` and sends the result
     * to JS.
     * @see
     * https://github.com/replayio/chromium-v8/blob/b38bf5b0b1f149f7af3fd90a2ce12344e7191d03/src/inspector/custom-preview.cc#L123
     */
    std::vector<uint8_t> json;
    auto errorMessage = Convert(cbor, json);
    if (!errorMessage.length()) {
      auto jsonStr =
          v8::String::NewFromOneByte(isolate, json.data(),
                                     v8::NewStringType::kNormal, (int)json.size())
              .ToLocalChecked();
      // see https://stackoverflow.com/a/23688325
      auto context = isolate->GetCurrentContext();
      auto jsonObj = v8::JSON::Parse(context, jsonStr);
      if (!jsonObj.IsEmpty()) {
        return jsonObj.ToLocalChecked();
      }
    } else {
      recordreplay::Warning("convertCborToJSTempl - Failed to deserialize: %s",
                            errorMessage.c_str());
    }
  }
  v8::MaybeLocal<v8::Value> defaultVal;
  return defaultVal;
}

std::string ConvertCborToJsonV8(const std::vector<uint8_t>& cbor,
                                std::vector<uint8_t>& json) {
  auto cborSpan = v8_crdtp::SpanFrom(cbor);
  auto status = v8_crdtp::json::ConvertCBORToJSON(cborSpan, &json);
  if (status.ok()) {
    return "";
  }
  return status.ToASCIIString();
}

v8::MaybeLocal<v8::Value> convertCborToJS(
    v8::Isolate* isolate,
    v8_crdtp::Serializable* value) {
  return convertCborToJSTempl<v8_crdtp::Serializable, ConvertCborToJsonV8>(
      isolate, value);
}

std::string ConvertCborToJsonDefault(const std::vector<uint8_t>& cbor,
                                std::vector<uint8_t>& json) {
  auto cborSpan = crdtp::SpanFrom(cbor);
  auto status = crdtp::json::ConvertCBORToJSON(cborSpan, &json);
  if (status.ok()) {
    return "";
  }
  return status.ToASCIIString();
}

v8::MaybeLocal<v8::Value> convertCborToJS(
    v8::Isolate* isolate,
    crdtp::Serializable* value) {
  return convertCborToJSTempl<crdtp::Serializable, ConvertCborToJsonDefault>(
      isolate, value);
}

template <typename T>
v8::Local<v8::Array> convertCborToJS(v8::Isolate* isolate,
                                     std::vector<std::unique_ptr<T>>* arr) {
  v8::Local<v8::Array> result = v8::Array::New(isolate);
  auto context = isolate->GetCurrentContext();
  for (uint32_t i = 0; i < arr->size(); ++i) {
    auto* entry = (crdtp::Serializable*)(*arr)[i].get();
    auto item =
        convertCborToJSTempl<crdtp::Serializable, ConvertCborToJsonDefault>(
            isolate, entry);
    if (!item.IsEmpty()) {
      result->Set(context, i, item.ToLocalChecked()).Check();
    } else {
      result->Set(context, i, Null(isolate)).Check();
    }
  }
  return result;
}

template <typename T>
v8::MaybeLocal<v8::Value> convertCborToJSMaybe(v8::Isolate* isolate,
                                               crdtp::Maybe<T> value) {
  static_assert(
      std::is_base_of<crdtp::Serializable, T>::value,
      "type parameter T of Maybe<T> must derive from crdtp::Serializable");

  if (value.isJust()) {
    crdtp::Serializable* serializable = (crdtp::Serializable*)value.fromJust();
    return convertCborToJSTempl<crdtp::Serializable, ConvertCborToJsonDefault>(
        isolate, serializable);
  }
  v8::MaybeLocal<v8::Value> defaultVal;
  return defaultVal;
}


/** ###########################################################################
 * More Debugger interfaces (Inspectors)
 * @see https://static.replay.io/protocol/tot/DOM/
 * @see https://chromedevtools.github.io/devtools-protocol/tot/DOM/
 * ##########################################################################*/


static InspectedFrames* getOrCreateInspectedFrames(v8::Isolate* isolate, int contextGroupId) {
  InspectorData *data = getInspectorFor(isolate, contextGroupId);

  if (!data->inspectedFrames) {
    data->inspectedFrames = MakeGarbageCollected<InspectedFrames>(data->GetLocalFrameRoot());
  }
  return data->inspectedFrames;
}

// NOTE: we need to instantiate all inspectors indivudally because we
//    are not fully hooked up with a `DevToolsSession` + `UberDispatcher`.
//    We also cannot enable them for the same reason.
absl::optional<InspectorDOMAgent*> getOrCreateInspectorDOMAgent(v8::Isolate* isolate) {
  auto contextGroupId = GetCurrentContextGroupIdForIsolate(isolate);
  if (!contextGroupId.has_value()) {
    return absl::optional<InspectorDOMAgent*>();
  }

  InspectorData *data = getInspectorFor(isolate, *contextGroupId);

  if (!data->inspectorDomAgent) {
    // NOTE: based on WebDevToolsAgentImpl::AttachSession
    InspectedFrames* inspectedFrames = getOrCreateInspectedFrames(isolate, *contextGroupId);
    data->inspectorDomAgent = MakeGarbageCollected<InspectorDOMAgent>(
        isolate, inspectedFrames, getInspectorSession(isolate, *contextGroupId));
    data->inspectorDomAgent->FrameDocumentUpdated(data->GetLocalFrameRoot());
  }
  return data->inspectorDomAgent;
}

absl::optional<InspectorDOMDebuggerAgent*> getOrCreateInspectorDOMDebuggerAgent(
    v8::Isolate* isolate) {
  auto contextGroupId = GetCurrentContextGroupIdForIsolate(isolate);

  if (!contextGroupId.has_value()) {
    return absl::optional<InspectorDOMDebuggerAgent*>();
  }

  InspectorData *data = getInspectorFor(isolate, *contextGroupId);

  if (!data->inspectorDomDebuggerAgent) {
    data->inspectorDomDebuggerAgent =
        MakeGarbageCollected<InspectorDOMDebuggerAgent>(
            isolate, *getOrCreateInspectorDOMAgent(isolate), getInspectorSession(isolate, *contextGroupId));

    // RUN-1061: registering the agent here allows it to receive `UserCallback`
    // events.
    data->GetLocalFrameRoot()->GetProbeSink()->AddInspectorDOMDebuggerAgent(data->inspectorDomDebuggerAgent);
  }
  return data->inspectorDomDebuggerAgent;
}

absl::optional<InspectorNetworkAgent*> getOrCreateInspectorNetworkAgent(v8::Isolate* isolate) {
  absl::optional<int> contextGroupId = GetCurrentContextGroupIdForIsolate(isolate);
  if (!contextGroupId.has_value()) {
    return absl::optional<InspectorNetworkAgent*>();
  }

  InspectorData *data = getInspectorFor(isolate, *contextGroupId);
  
  if (!data->inspectorNetworkAgent) {
    InspectedFrames* inspectedFrames = getOrCreateInspectedFrames(isolate, *contextGroupId);
    data->inspectorNetworkAgent = MakeGarbageCollected<InspectorNetworkAgent>(
        inspectedFrames, nullptr, getInspectorSession(isolate, *contextGroupId));
  }
  return data->inspectorNetworkAgent;
}

absl::optional<InspectorCSSAgent*> getOrCreateInspectorCSSAgent(v8::Isolate* isolate) {
  absl::optional<int> contextGroupId = GetCurrentContextGroupIdForIsolate(isolate);
  if (!contextGroupId.has_value()) {
    return absl::optional<InspectorCSSAgent*>();
  }

  InspectorData *data = getInspectorFor(isolate, *contextGroupId);

  if (!data->inspectorCssAgent) {
    // NOTE: based on WebDevToolsAgentImpl::AttachSession
    InspectedFrames* inspectedFrames = getOrCreateInspectedFrames(isolate, *contextGroupId);

    auto* resource_content_loader =
        MakeGarbageCollected<InspectorResourceContentLoader>(data->GetLocalFrameRoot());
    auto* resource_container =
        MakeGarbageCollected<InspectorResourceContainer>(inspectedFrames);
    auto domAgent = getOrCreateInspectorDOMAgent(isolate);

    auto* networkAgent = *getOrCreateInspectorNetworkAgent(isolate);
    data->inspectorCssAgent = MakeGarbageCollected<InspectorCSSAgent>(
        *domAgent, inspectedFrames, networkAgent, resource_content_loader,
        resource_container);

    // NOTE: we cannot easily enable without a full session active,
    //      but if we wanted to, here is an example:
    // https://source.chromium.org/chromium/chromium/src/+/main:out/mac-Debug/gen/third_party/blink/renderer/core/inspector/protocol/css.cc;l=890?q=EnableCallbackImpl&ss=chromium%2Fchromium%2Fsrc
    // std::unique_ptr<blink::protocol::CSS::Backend::EnableCallback>
    // cb(nullptr); gInspectorCssAgent->enable(std::move(cb));
  }
  return data->inspectorCssAgent;
}

/** ###########################################################################
 * Object Management
 * ##########################################################################*/

static bool
getObjectByCdpId(v8::Isolate* isolate,
                  const v8_inspector::StringView& cdpIdV8,
                  v8::Local<v8::Object>& plainObject) {
  auto context = isolate->GetCurrentContext();
  std::unique_ptr<v8_inspector::StringBuffer> error;
  v8::Local<v8::Value> unwrapped;

  absl::optional<int> contextGroupId = GetCurrentContextGroupIdForIsolate(isolate);
  if (!contextGroupId.has_value()) {
    recordreplay::Warning("[RUN-2600] getObjectByCdpId - Failed to find contextGroupId");
    return false;
  }

  if (!getInspectorSession(isolate, *contextGroupId)->unwrapObject(&error, cdpIdV8, &unwrapped, &context,
                                       nullptr)) {
    recordreplay::Warning("getObjectByCdpId - Failed to look up cdpId: %s",
                        ToCoreString(error->string()).Ascii().c_str());
    return false;
  }
  plainObject = unwrapped.As<v8::Object>();
  return true;
}

/**
 * Returns the matching object or null.
 * Should generally never return null.
 */
static bool getV8FromBlinkObject(
    v8::Isolate* isolate,
    ScriptWrappable* blinkObject,
    v8::Local<v8::Value>& result) {
  ScriptState* scriptState = ScriptState::Current(isolate);
  v8::Local<v8::Value> v8Object;
  if (blinkObject->Wrap(scriptState).ToLocal(&v8Object)) {
    result = v8Object;
    return true;
  }

  // weird
  recordreplay::Print("[RuntimeError] getV8FromBlinkObject failed");
  return false;
}

/**
 * NOTE: Since the `RemoteObject` type is not publicly exposed, we cannot easily
 * access it in CPP space. We thus only use it in JS. This basically emulates
 * gecko's `makeDebuggeeValue`.
 */
static void fromJsMakeDebuggeeValue(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  CHECK(args.Length() == 1 &&
        "must be called with a single value");

  auto contextGroupId = GetCurrentContextGroupIdForIsolate(isolate);

  if (!contextGroupId.has_value()) {
      recordreplay::Warning("[RUN-2600] fromJsMakeDebuggeeValue - no valid context id");
      args.GetReturnValue().SetNull();
      return;
  }

  auto context = isolate->GetCurrentContext();
  auto value = args[0];

  const String object_group(REPLAY_CDT_PAUSE_OBJECT_GROUP);
  auto generatePreview = false;

  // NOTE: `wrapObject` always creates a new `RemoteObject` and binds it
  // to a new id.
  auto remoteObjSerialized = getInspectorSession(isolate, *contextGroupId)->wrapObject(
      context, value, ToV8InspectorStringView(object_group), generatePreview);

  if (remoteObjSerialized) {
    auto result = convertCborToJS(isolate, (v8_crdtp::Serializable*)remoteObjSerialized.get());

    if (!result.IsEmpty()) {
      args.GetReturnValue().Set(result.ToLocalChecked());
      return;
    }
  }
  args.GetReturnValue().SetNull();
}

static void fromJsGetArgumentsInFrame(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() &&
        "must be called with a single string");
  v8::Isolate* isolate = args.GetIsolate();
  auto contextGroupId = GetCurrentContextGroupIdForIsolate(isolate);

  if (!contextGroupId.has_value()) {
    recordreplay::Warning("[RUN-2600] fromJsGetArgumentsInFrame - no valid context id");
    args.GetReturnValue().SetNull();
    return;
  }

  // convert v8::String → v8::String::Utf8Value → v8_inspector::StringView
  // future-work: can this be improved?
  v8::String::Utf8Value frameId(isolate, args[0]);
  const uint8_t* frameIdPtr = reinterpret_cast<const uint8_t*>(*frameId);
  v8_inspector::StringView frameIdV8(frameIdPtr, frameId.length());

  auto result = getInspectorSession(isolate, *contextGroupId)->getArgumentsOfCallFrame(frameIdV8);

  if (result.IsEmpty()) {
    args.GetReturnValue().SetNull();
  } else {
    args.GetReturnValue().Set(result.ToLocalChecked());
  }
}

static void fromJsGetObjectByCdpId(
  const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() &&
        "[RuntimeError] must be called with a single string");

  v8::Isolate* isolate = args.GetIsolate();

  // convert v8::String → v8::String::Utf8Value → v8_inspector::StringView
  // future-work: can this be improved?
  v8::String::Utf8Value cdpId(isolate, args[0]);
  const uint8_t* cdpIdPtr = reinterpret_cast<const uint8_t*>(*cdpId);
  v8_inspector::StringView cdpIdV8(cdpIdPtr, cdpId.length());

  v8::Local<v8::Object> plainObject;
  if (getObjectByCdpId(isolate, cdpIdV8, plainObject)) {
    args.GetReturnValue().Set(plainObject);
  } else {
    args.GetReturnValue().SetNull();
  }
}

/**
 * Whether a given value is a blink object.
 *
 * NOTE: If we want a generalized |isNativeObject| function,
 * we probably have to look at |v8::internal::Script::type|
 * (which is also used by |CallSiteInfo::IsNative|).
 */
static void fromJsIsBlinkObject(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 &&
        "[RuntimeError] must be called with a single value");

  v8::Isolate* isolate = args.GetIsolate();

  bool result = V8DOMWrapper::IsWrapper(isolate, args[0]);

  args.GetReturnValue().Set(result);
}

/** ###########################################################################
 * Networking
 * ##########################################################################*/

// Represents a known network request.  Created and added to
// `gActiveNetworkRequests` when the request is first seen.  Removed
// when the request finishes or fails.
struct NetworkRequestStatus {
  size_t response_data_received;
  size_t request_data_sent;
  base::Value info;
  NetworkRequestStatus(const base::DictionaryValue& info_arg)
  : response_data_received(0),
    request_data_sent(0),
    info(info_arg.Clone())
  {}
};
// Map of active network requests.
std::unordered_map<std::string, NetworkRequestStatus>*
  gActiveNetworkRequests = nullptr;

// Globals storing values to be returned to controller commands
// `GetCurrentNetwork*`
static base::Value *gCurrentNetworkRequestEvent = nullptr;
static std::vector<uint8_t>* gCurrentNetworkStreamData = nullptr;

static void GetCurrentNetworkRequestEvent(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (!gCurrentNetworkRequestEvent) {
    return;
  }

  v8::Isolate* isolate = args.GetIsolate();
  std::string json;
  base::JSONWriter::Write(*gCurrentNetworkRequestEvent, &json);
  v8::Local<v8::String> json_string = ToV8String(isolate, json.c_str());
  args.GetReturnValue().Set(json_string);
}

static void GetCurrentNetworkStreamData(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(gCurrentNetworkStreamData);

  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  // Expect params: { index, length }
  v8::Local<v8::Object> params =
    args[0]->ToObject(context).ToLocalChecked();
  size_t index =
    params->Get(context, ToV8String(isolate, "index"))
      .ToLocalChecked()->NumberValue(context).ToChecked();
  size_t length =
    params->Get(context, ToV8String(isolate, "length"))
      .ToLocalChecked()->NumberValue(context).ToChecked();
  size_t size = gCurrentNetworkStreamData->size();

  if ((size < index) || ((size - index) < length)) {
    recordreplay::Print(
      "GetCurrentNetworkStreamData: Out of range slice"
      " (size=%u, requested=%u-%u)",
      (unsigned) size,
      (unsigned) index,
      (unsigned) (index + length)
    );
    return;
  }

  uint8_t* bytes = &(*gCurrentNetworkStreamData)[index];
  std::string encoded = base::Base64Encode(
    base::span<const uint8_t>(bytes, length)
  );
  char* encoded_cstr = strdup(encoded.c_str());
  char* encoded_end = encoded_cstr + encoded.length();
  for (char *cur = encoded_cstr; cur < encoded_end; cur++) {
    if (*cur == '-') { *cur = '+'; }
    if (*cur == '_') { *cur = '/'; }
  }

  v8::Local<v8::Object> result = v8::Object::New(isolate);
  result->Set(context,
    ToV8String(isolate, "kind"),
    ToV8String(isolate, "data")
  ).Check();
  result->Set(context,
    ToV8String(isolate, "value"),
    ToV8String(isolate, encoded_cstr)
  ).Check();
  args.GetReturnValue().Set(result);
}

static std::string MakeRequestIdentifier(uint64_t identifier) {
  char request_id[64];
  snprintf(request_id, 64, "%d.%lu", (int) base::GetCurrentProcId(), (unsigned long) identifier);
  return std::string(request_id);
}

static std::string GetRequestIdentifierProperty(const base::DictionaryValue& info) {
  uint64_t identifier =
    *info.FindPath("identifier")->GetIfDouble();
  return MakeRequestIdentifier(identifier);
}

static void CopyDictionaryProperty(base::DictionaryValue& dst,
                                   const base::DictionaryValue& src,
                                   const char* property) {
  const base::Value* value = src.FindPath(property);
  if (value) {
    dst.Set(property, std::unique_ptr<base::Value>(value->CreateDeepCopy()));
  }
}

static void HandleNetworkPrepareRequestEvent(const base::DictionaryValue& info) {
  CHECK(gActiveNetworkRequests);
  std::string request_id = GetRequestIdentifierProperty(info);
  if (gActiveNetworkRequests->find(request_id) != gActiveNetworkRequests->end()) {
    // If the request already exists, this is a redirect.
    // Chromium will send a "Network.ResourceRedirect" event which will
    // be handled by `HandleNetworkPrepareRequestEvent` below.
    return;
  }

  // Save request info in a global table.
  // Associate with it the original request info which may be needed later if the
  // request is redirected.
  gActiveNetworkRequests->insert(
    { request_id, NetworkRequestStatus(info) }
  );

  // Register the request.
  uint64_t bookmark = *info.FindPath("bookmark")->GetIfDouble();
  recordreplay::OnNetworkRequest(request_id.c_str(), "http", bookmark);

  // Package and emit a network request event with the appropriate info.
  base::DictionaryValue event;
  event.SetString("kind", "request");
  CopyDictionaryProperty(event, info, "requestUrl");
  CopyDictionaryProperty(event, info, "requestHeaders");
  CopyDictionaryProperty(event, info, "requestMethod");
  CopyDictionaryProperty(event, info, "requestCause");
  CopyDictionaryProperty(event, info, "initiator");

  gCurrentNetworkRequestEvent = &event;
  recordreplay::OnNetworkRequestEvent(request_id.c_str());
  gCurrentNetworkRequestEvent = nullptr;
}

static void HandleNetworkResourceRedirectEvent(const base::DictionaryValue& info) {
  CHECK(gActiveNetworkRequests);

  // Retrieve the existing request data which should already have been
  // registered by `HandleNetworkPrepareRequestEvent`.
  std::string request_id = GetRequestIdentifierProperty(info);
  auto request_info = gActiveNetworkRequests->find(request_id);
  if (request_info == gActiveNetworkRequests->end()) {
    recordreplay::Print("No original request for navigation redirect: %s",
      request_id.c_str());
    return;
  }
  const base::DictionaryValue& original_info =
    base::Value::AsDictionaryValue(request_info->second.info);

  // Register a new network request with the same request id as the original
  // for this redirect.
  uint64_t bookmark = *original_info.FindPath("bookmark")->GetIfDouble();
  recordreplay::OnNetworkRequest(request_id.c_str(), "http", bookmark);

  // Package and emit a network request event, using data from the original
  // request when necessary.
  base::DictionaryValue event;
  event.SetString("kind", "request");
  CopyDictionaryProperty(event, info, "requestUrl");
  CopyDictionaryProperty(event, info, "requestHeaders");
  CopyDictionaryProperty(event, original_info, "requestMethod");
  CopyDictionaryProperty(event, original_info, "requestCause");
  CopyDictionaryProperty(event, original_info, "initiator");

  gCurrentNetworkRequestEvent = &event;
  recordreplay::OnNetworkRequestEvent(request_id.c_str());
  gCurrentNetworkRequestEvent = nullptr;
}

static void HandleNetworkNavigationEvent(const base::DictionaryValue& info) {
  CHECK(gActiveNetworkRequests);

  // Navigation events are network requests that are not resource requests.
  // They are directed here (the renderer process) from the content process.
  // They have no associated bookmark, as we can't take bookmarks in the
  // content process.

  // Ensure that a request with the same ID has not already been registered.
  std::string request_id = *info.FindPath("requestId")->GetIfString();
  if (gActiveNetworkRequests->find(request_id) != gActiveNetworkRequests->end()) {
    recordreplay::Print("Duplicate request id: %s", request_id.c_str());
    return;
  }
  gActiveNetworkRequests->insert({ request_id, NetworkRequestStatus(info) });

  // A navigation event is a new network request, so call the `OnNetworkRequest` hook.
  // Navigation events have no bookmarks associated with them.
  recordreplay::OnNetworkRequest(request_id.c_str(), "http", /* bookmark = */ 0);

  // Package and emit a network request event.
  base::DictionaryValue event;
  event.SetString("kind", "request");
  CopyDictionaryProperty(event, info, "requestUrl");
  CopyDictionaryProperty(event, info, "requestHeaders");
  CopyDictionaryProperty(event, info, "requestMethod");
  event.SetString("requestCause", "document");

  gCurrentNetworkRequestEvent = &event;
  recordreplay::OnNetworkRequestEvent(request_id.c_str());
  gCurrentNetworkRequestEvent = nullptr;
}

static void HandleNetworkNavigationRedirectEvent(const base::DictionaryValue& info) {
  CHECK(gActiveNetworkRequests);

  // Navigation redirect events are, as with navigation events, sent from
  // the content process to the renderer process.

  // Ensure that a request with the same ID has not already been registered.
  std::string request_id = *info.FindPath("requestId")->GetIfString();
  // This is a redirect, so an existing request should have been registered
  // with the same id.
  auto request_info = gActiveNetworkRequests->find(request_id);
  if (request_info == gActiveNetworkRequests->end()) {
    recordreplay::Print("No original request for navigation redirect: %s",
      request_id.c_str());
    return;
  }
  const base::DictionaryValue& original_info =
    base::Value::AsDictionaryValue(request_info->second.info);

  // A navigation redirect event is a new network request. There is no bookmark.
  recordreplay::OnNetworkRequest(request_id.c_str(), "http", 0);

  // Package and emit a network request event.
  // The request method is obtained from the saved request info.
  base::DictionaryValue event;
  event.SetString("kind", "request");
  CopyDictionaryProperty(event, info, "requestUrl");
  CopyDictionaryProperty(event, info, "requestHeaders");
  CopyDictionaryProperty(event, original_info, "requestMethod");
  event.SetString("requestCause", "document");

  gCurrentNetworkRequestEvent = &event;
  recordreplay::OnNetworkRequestEvent(request_id.c_str());
  gCurrentNetworkRequestEvent = nullptr;
}

static void HandleNetworkRequestDataFormEvent(const base::DictionaryValue& info) {
  CHECK(gActiveNetworkRequests);
  std::string request_id = GetRequestIdentifierProperty(info);
  auto request_info = gActiveNetworkRequests->find(request_id);
  if (request_info == gActiveNetworkRequests->end()) {
    recordreplay::Print("Unknown request for request data: %s",
      request_id.c_str());
    return;
  }

  // If we're receiving a RequestData.Form event, all the
  // request data is present and none should have been already received.
  CHECK(request_info->second.request_data_sent == 0);

  { // Send a "request-body" network request event.
    base::DictionaryValue requestBodyEvent;
    requestBodyEvent.SetString("kind", "request-body");

    gCurrentNetworkRequestEvent = &requestBodyEvent;
    recordreplay::OnNetworkRequestEvent(request_id.c_str());
    gCurrentNetworkRequestEvent = nullptr;
  }

  std::string stream_id = "request-" + request_id;

  // Call StreamStart API.
  recordreplay::OnNetworkStreamStart(
    stream_id.c_str(), "request-data", request_id.c_str()
  );

  // Call StreamData API.
  size_t length = *info.FindPath("dataLength")->GetIfDouble();

  CHECK(length >= 0);
  gCurrentNetworkStreamData->clear();
  const std::string *data_base64 = info.FindPath("data")->GetIfString();
  if (data_base64) {
    const uint8_t* data =
      reinterpret_cast<const uint8_t *>(data_base64->c_str());
    gCurrentNetworkStreamData->insert(
      gCurrentNetworkStreamData->begin(),
      data,
      data + data_base64->length()
    );
    size_t offset = request_info->second.response_data_received;
    recordreplay::OnNetworkStreamData(
      stream_id.c_str(), offset, length, /* bookmark = */ 0
    );
    gCurrentNetworkStreamData->clear();
  }
  request_info->second.request_data_sent += length;
}

static void HandleNetworkDidReceiveResponseEvent(const base::DictionaryValue& info) {
  CHECK(gActiveNetworkRequests);
  std::string request_id = GetRequestIdentifierProperty(info);
  auto request_info = gActiveNetworkRequests->find(request_id);
  if (request_info == gActiveNetworkRequests->end()) {
    recordreplay::Print("Unknown request received response: %s",
      request_id.c_str());
    return;
  }

  base::DictionaryValue event;
  event.SetString("kind", "response");
  CopyDictionaryProperty(event, info, "responseHeaders");
  CopyDictionaryProperty(event, info, "responseProtocolVersion");
  CopyDictionaryProperty(event, info, "responseStatus");
  CopyDictionaryProperty(event, info, "responseStatusText");
  CopyDictionaryProperty(event, info, "responseFromCache");

  gCurrentNetworkRequestEvent = &event;
  recordreplay::OnNetworkRequestEvent(request_id.c_str());
  gCurrentNetworkRequestEvent = nullptr;
}

static void HandleNetworkDidFinishLoadingEvent(const base::DictionaryValue& info) {
  CHECK(gActiveNetworkRequests);
  std::string request_id = GetRequestIdentifierProperty(info);
  auto request_info = gActiveNetworkRequests->find(request_id);
  if (request_info == gActiveNetworkRequests->end()) {
    recordreplay::Print("Unknown request finished loading: %s",
      request_id.c_str());
    return;
  }

  base::DictionaryValue event;
  event.SetString("kind", "request-done");
  CopyDictionaryProperty(event, info, "encodedBodySize");
  CopyDictionaryProperty(event, info, "decodedBodySize");

  gCurrentNetworkRequestEvent = &event;
  recordreplay::OnNetworkRequestEvent(request_id.c_str());
  gCurrentNetworkRequestEvent = nullptr;
}

static void HandleNetworkDidFailLoadingEvent(const base::DictionaryValue& info) {
  CHECK(gActiveNetworkRequests);
  std::string request_id = GetRequestIdentifierProperty(info);
  auto request_info = gActiveNetworkRequests->find(request_id);
  if (request_info == gActiveNetworkRequests->end()) {
    recordreplay::Print("Unknown request failed loading: %s",
      request_id.c_str());
    return;
  }

  base::DictionaryValue event;
  event.SetString("kind", "request-failed");
  CopyDictionaryProperty(event, info, "requestFailedReason");

  gCurrentNetworkRequestEvent = &event;
  recordreplay::OnNetworkRequestEvent(request_id.c_str());
  gCurrentNetworkRequestEvent = nullptr;
}

static void HandleNetworkDidReceiveDataEvent(const base::DictionaryValue& info) {
  CHECK(gActiveNetworkRequests);
  CHECK(gCurrentNetworkStreamData);
  // Get request info.
  std::string request_id = GetRequestIdentifierProperty(info);
  auto request_info = gActiveNetworkRequests->find(request_id);
  if (request_info == gActiveNetworkRequests->end()) {
    recordreplay::Print("Unknown request received data: %s",
      request_id.c_str());
    return;
  }

  std::string stream_id = "response-" + request_id;

  // The first byte of data received triggers a "response-body" event.
  if (request_info->second.response_data_received == 0) {
    base::DictionaryValue event;
    event.SetString("kind", "response-body");

    gCurrentNetworkRequestEvent = &event;
    recordreplay::OnNetworkRequestEvent(request_id.c_str());
    gCurrentNetworkRequestEvent = nullptr;

    recordreplay::OnNetworkStreamStart(
      stream_id.c_str(), "response-data", request_id.c_str()
    );
  }

  // Sending stream data.
  size_t length = *info.FindPath("dataLength")->GetIfDouble();
  CHECK(length >= 0);

  gCurrentNetworkStreamData->clear();
  const std::string *data_base64 = info.FindPath("data")->GetIfString();
  if (data_base64) {
    std::string out_string;
    if (!base::Base64Decode(*data_base64, &out_string)) {
      recordreplay::Print("Unknown request received data: %s",
        request_id.c_str());
      return;
    }
    const uint8_t* data =
      reinterpret_cast<const uint8_t *>(out_string.c_str());
    gCurrentNetworkStreamData->insert(
      gCurrentNetworkStreamData->begin(),
      data,
      data + out_string.length()
    );
    size_t offset = request_info->second.response_data_received;
    recordreplay::OnNetworkStreamData(
      stream_id.c_str(), offset, length, /* bookmark = */ 0
    );
    gCurrentNetworkStreamData->clear();
  }
  request_info->second.response_data_received += length;
}

/** ###########################################################################
 * blink (DOM, CSS etc.) queries
 * @see https://static.replay.io/protocol/tot/DOM/
 * @see https://chromedevtools.github.io/devtools-protocol/tot/DOM/
 * ##########################################################################*/

static bool checkCDPResponse(const char* label,
                             const protocol::Response& response,
                             const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (!response.IsSuccess()) {
    recordreplay::Warning(
        "CDP %s failed (Code: %d): %s",
        label,
        response.Code(),
        response.Message().c_str());

    // result is null
    args.GetReturnValue().SetNull();
    return false;
  }
  return true;
}

static void fromJsGetNodeIdByCpdId(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() &&
        "[RuntimeError] must be called with a single string");

  v8::Isolate* isolate = args.GetIsolate();

  // convert v8::String → v8::String::Utf8Value → v8_inspector::StringView
  v8::String::Utf8Value cdpId(isolate, args[0]);
  const uint8_t* cdpIdPtr = reinterpret_cast<const uint8_t*>(*cdpId);
  v8_inspector::StringView cdpIdV8(cdpIdPtr, cdpId.length());

  v8::Local<v8::Object> nodeObj;
  if (getObjectByCdpId(isolate, cdpIdV8, nodeObj)) {
    Node* node = V8Node::ToImplWithTypeCheck(isolate, nodeObj);
    if (node) {
      // Bind node and get nodeId.
      auto domAgent = getOrCreateInspectorDOMAgent(isolate);
      if (!domAgent.has_value()) {
        recordreplay::CommandDiagnostic("fromJsGetNodeIdByCpdId no context id.");
        args.GetReturnValue().SetNull();
        return;
      }

      int nodeId = (*domAgent)->BindDocumentNode(node);
      args.GetReturnValue().Set(v8::Number::New(isolate, nodeId));
      return;
    } else {
      // This should be reported by the caller, where we have more relevant
      // context info.
    }
  } else { /* already reported by getObjectByCdpId */ }

  args.GetReturnValue().SetNull();
}


static void fromJsGetBoxModel(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsNumber() &&
        "[RuntimeError] must be called with a single number");

  v8::Isolate* isolate = args.GetIsolate();
  auto nodeId = (int)args[0].As<v8::Integer>()->Value();

  auto domAgent = getOrCreateInspectorDOMAgent(isolate);
  if (!domAgent.has_value()) {
    recordreplay::CommandDiagnostic("CDP InspectorDOMAgent.getBoxModel no context id.");
    args.GetReturnValue().SetNull();
    return;
  }

  int backend_node_id = 0;
  String object_id;
  std::unique_ptr<protocol::DOM::BoxModel> boxModel;
  auto response =
      (*domAgent)->getBoxModel(nodeId, backend_node_id, object_id, &boxModel);

  if (!response.IsSuccess()) {
    // This can happen when querying nodes that don't have a box model.
    recordreplay::CommandDiagnostic(
        "CDP InspectorDOMAgent.getBoxModel failed (nodeId: %d, Code: "
        "%d): %s",
        nodeId, response.Code(), response.Message().c_str());
  } else {
    auto result = convertCborToJS(isolate, boxModel.get());
    if (!result.IsEmpty()) {
      args.GetReturnValue().Set(result.ToLocalChecked());
      return;
    }
  }

  args.GetReturnValue().SetNull();
}


static void fromJsGetMatchedStylesForElement(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsNumber() &&
        "[RuntimeError] must be called with a single number");

  v8::Isolate* isolate = args.GetIsolate();
  auto nodeId = (int)args[0].As<v8::Integer>()->Value();

  auto cssAgent = getOrCreateInspectorCSSAgent(isolate);
  if (!cssAgent.has_value()) {
    recordreplay::Warning("[RUN-2600] fromJsGetMatchedStylesForElement failed no context id");
    args.GetReturnValue().SetNull();
    return;
  }

  Maybe<protocol::CSS::CSSStyle> inlineStyle;
  Maybe<protocol::CSS::CSSStyle> attributesStyle;
  Maybe<protocol::Array<protocol::CSS::RuleMatch>> matchedRules;
  Maybe<protocol::Array<protocol::CSS::PseudoElementMatches>> pseudoIdMatches;
  Maybe<protocol::Array<protocol::CSS::InheritedStyleEntry>> inheritedEntries;
  Maybe<protocol::Array<protocol::CSS::InheritedPseudoElementMatches>> inherited_pseudo_id_matches;
  Maybe<protocol::Array<protocol::CSS::CSSKeyframesRule>> keyframesRules;
  Maybe<int> parentLayoutNodeId;

  auto response = (*cssAgent)->getMatchedStylesForNode(
    nodeId, &inlineStyle, &attributesStyle, &matchedRules, &pseudoIdMatches,
    &inheritedEntries, &inherited_pseudo_id_matches, &keyframesRules,
    &parentLayoutNodeId);

  if (!response.IsSuccess()) {
    recordreplay::Warning(
        "CDP CSS.getMatchedStylesForNode failed (nodeId: %d, Code: "
        "%d): %s",
        nodeId, response.Code(), response.Message().c_str());
    args.GetReturnValue().SetNull();
    return;
  }

  v8::Local<v8::Object> result = v8::Object::New(isolate);
  // NOTE: not sure what `attributesStyle` is and how its different from `inlineStyle`?
  if (attributesStyle.isJust()) {
    auto rulesJs = convertCborToJS(isolate, attributesStyle.fromJust());
    if (!rulesJs.IsEmpty()) {
      SetDataProperty(isolate, result, "attributesStyle",
                      rulesJs.ToLocalChecked());
    }
  }
  if (matchedRules.isJust()) {
    auto rulesJs = convertCborToJS(isolate, matchedRules.fromJust());
    SetDataProperty(isolate, result, "matchedRules", rulesJs);
  }
  if (pseudoIdMatches.isJust()) {
    auto rulesJs = convertCborToJS(isolate, pseudoIdMatches.fromJust());
    SetDataProperty(isolate, result, "pseudoIdMatches", rulesJs);
  }
  if (inheritedEntries.isJust()) {
    auto rulesJs = convertCborToJS(isolate, inheritedEntries.fromJust());
    SetDataProperty(isolate, result, "inheritedEntries", rulesJs);
  }
  if (keyframesRules.isJust()) {
    auto rulesJs = convertCborToJS(isolate, keyframesRules.fromJust());
    SetDataProperty(isolate, result, "keyframesRules", rulesJs);
  }
  args.GetReturnValue().Set(result);
}


static void fromJsCssGetStylesheetByCpdId(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() &&
        "[RuntimeError] must be called with a single string");

  v8::Isolate* isolate = args.GetIsolate();

  auto sheetId = ToCoreString(args[0].As<v8::String>());
  auto cssAgent = getOrCreateInspectorCSSAgent(isolate);
  if (!cssAgent.has_value()) {
    recordreplay::Warning("[RUN-2600] fromJsCssGetStylesheetByCpdId failed no context id");
    args.GetReturnValue().SetNull();
    return;
  }

  CSSStyleSheet* styleSheet = (*cssAgent)->getStyleSheet(sheetId);
  v8::Local<v8::Value> v8StyleSheet;
  if (styleSheet && getV8FromBlinkObject(isolate, styleSheet, v8StyleSheet)) {
    args.GetReturnValue().Set(v8StyleSheet);
  } else {
    args.GetReturnValue().SetNull();
  }
}

static void fromJsDomPerformSearch(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() &&
        "[RuntimeError] must be called with a single string");

  v8::Isolate* isolate = args.GetIsolate();

  auto query = ToCoreString(args[0].As<v8::String>());
  auto domAgent = getOrCreateInspectorDOMAgent(isolate);
  if (!domAgent.has_value()) {
    recordreplay::Warning("[RUN-2600] fromJsDomPerformSearch failed no context id");
    return;
  }

  bool includeUserAgentShadowDom = true;
  String searchId;
  int resultCount;

  // NOTE: We modified performSearch to work even though the agent is not
  // enabled.
  auto response = (*domAgent)->performSearch(query, includeUserAgentShadowDom,
                                          &searchId, &resultCount);
  if (checkCDPResponse("DOM.performSearch", response, args)) {
    if (resultCount) {
      int fromIndex = 0;
      int toIndex = resultCount;
      std::unique_ptr<protocol::Array<int>> nodeIds;
      response =
          (*domAgent)->getSearchResults(searchId, fromIndex, toIndex, &nodeIds);
      if (checkCDPResponse("DOM.getSearchResults", response, args)) {
        v8::Local<v8::Array> result = v8::Array::New(isolate);
        uint32_t nWritten = 0;
        for (uint32_t i = 0; i < nodeIds->size(); ++i) {
          int nodeId = (*nodeIds)[i];
          auto* node = (*domAgent)->NodeForId(nodeId);
          v8::Local<v8::Value> v8Node;
          if (node && getV8FromBlinkObject(isolate, node, v8Node)) {
            v8::Local<v8::Context> context = isolate->GetCurrentContext();
            result->Set(context, nWritten++, v8Node).Check();
          }
        }
        args.GetReturnValue().Set(result);
      }
    } else {
      v8::Local<v8::Array> result = v8::Array::New(isolate);
      args.GetReturnValue().Set(result);
    }

    // clean up
    (*domAgent)->discardSearchResults(searchId);
  }
}

static void fromJsCollectEventListeners(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsObject() &&
        "[RuntimeError] must be called with a single plain object (DOM node)");

  v8::Isolate* isolate = args.GetIsolate();
  auto context = isolate->GetCurrentContext();
  auto nodeObject = args[0].As<v8::Object>();
  auto* node = V8Node::ToImplWithTypeCheck(isolate, nodeObject);

  v8::Local<v8::Array> result = v8::Array::New(isolate);
  if (!node) {
    recordreplay::Warning("[RUN-2282] JS fromJsCollectEventListeners: invalid argument is not blink Node");
  } else {
    auto report_for_all_contexts = true;
    V8EventListenerInfoList eventListenerInfos;
    InspectorDOMDebuggerAgent::CollectEventListeners(
        isolate, node, nodeObject, node, report_for_all_contexts,
        &eventListenerInfos);

    uint32_t i = 0;
    for (const auto& info : eventListenerInfos) {
      auto v8Info = v8::Object::New(isolate);
      SetDataProperty(isolate, v8Info, "type",
                      V8String(isolate, info.event_type));
      SetDataProperty(isolate, v8Info, "capture",
                      v8::Boolean::New(isolate, info.use_capture));
      SetDataProperty(isolate, v8Info, "handler", info.effective_function);
      result->Set(context, i++, v8Info).Check();
    }
  }
  args.GetReturnValue().Set(result);
}

static void fromJsGetFunctionBytecode(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsObject() &&
        "[RuntimeError] must be called with a single plain object");

  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Object> paramObj = args[0].As<v8::Object>();

  v8::Local<v8::Object> rv = v8::internal::RecordReplayGetBytecode(isolate, paramObj);

  args.GetReturnValue().Set(rv);
}

static void fromJsBeginReplayCode(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() &&
        "[RuntimeError] must be called with a single string");

  v8::String::Utf8Value label(args.GetIsolate(), args[0]);
  recordreplay::BeginDisallowEventsWithLabel(*label);
  recordreplay::EnterReplayCode();
}

static void fromJsEndReplayCode(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  recordreplay::EndDisallowEvents();
  recordreplay::ExitReplayCode();
}

/** ###########################################################################
 * misc
 * ##########################################################################*/

// Handle incoming browser events.
static void HandleBrowserEvent(const char* name, const char* payload) {
  base::Value val = base::JSONReader::Read(payload).value_or(base::Value());
  assert(!val.is_none() && "Browser event JSON failed");
  assert(!val.is_dict() && "Browser event JSON is not a dictionary");
  if (!strcmp(name, "Network.PrepareRequest")) {
    HandleNetworkPrepareRequestEvent(base::Value::AsDictionaryValue(val));
  } else if (!strcmp(name, "Network.ResourceRedirect")) {
    HandleNetworkResourceRedirectEvent(base::Value::AsDictionaryValue(val));
  } else if (!strcmp(name, "Network.RequestData.Form")) {
    HandleNetworkRequestDataFormEvent(base::Value::AsDictionaryValue(val));
  } else if (!strcmp(name, "Network.DidReceiveResponse")) {
    HandleNetworkDidReceiveResponseEvent(base::Value::AsDictionaryValue(val));
  } else if (!strcmp(name, "Network.DidFinishLoading")) {
    HandleNetworkDidFinishLoadingEvent(base::Value::AsDictionaryValue(val));
  } else if (!strcmp(name, "Network.DidFailLoading")) {
    HandleNetworkDidFailLoadingEvent(base::Value::AsDictionaryValue(val));
  } else if (!strcmp(name, "Network.DidReceiveData")) {
    HandleNetworkDidReceiveDataEvent(base::Value::AsDictionaryValue(val));
  } else if (!strcmp(name, "Network.Navigation")) {
    HandleNetworkNavigationEvent(base::Value::AsDictionaryValue(val));
  } else if (!strcmp(name, "Network.NavigationRedirect")) {
    HandleNetworkNavigationRedirectEvent(base::Value::AsDictionaryValue(val));
  } else {
    recordreplay::Print("HandleBrowserEvent received unrecognized event %s", name);
  }
}

// Called from page page javascript.
// `function __RECORD_REPLAY_ANNOTATION_HOOK__(kind, contents)`
// Since this function is called from userland JS, we avoid assertions.
// We don't want flawed uses of the API to crash the recording.
static void InvokeOnAnnotation(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (! (args.Length() >= 2 && args[0]->IsString())) {
    recordreplay::Print("[RuntimeError] %s called with incorrect arguments",
                        AnnotationHookJSName);
    return;
  }

  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Object> payload = v8::Object::New(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  payload->Set(context, ToV8String(isolate, "message"), args[1]).Check();

  v8::Local<v8::String> json;
  if (!v8::JSON::Stringify(context, payload).ToLocal(&json)) {
    recordreplay::Print("[RuntimeError] %s contents failed to json stringify",
                        AnnotationHookJSName);
    return;
  }

  v8::String::Utf8Value kind(args.GetIsolate(), args[0]);
  v8::String::Utf8Value contents(args.GetIsolate(), json);
  recordreplay::OnAnnotation(*kind, *contents);
}

extern "C" void V8RecordReplaySetAPIObjectIdCallback(int (*callback)(v8::Local<v8::Object>));
extern "C" void V8RecordReplayRegisterBrowserEventCallback(
  void (*callback)(const char* name, const char* payload)
);

/**
 * Copied from gin/try_catch.h.
 */
static v8::Local<v8::String> GetSourceLine(v8::Isolate* isolate,
                                    v8::Local<v8::Message> message) {
  auto maybe = message->GetSourceLine(isolate->GetCurrentContext());
  v8::Local<v8::String> source_line;
  return maybe.ToLocal(&source_line) ? source_line : v8::String::Empty(isolate);
}

static const std::string V8ToString(v8::Isolate* isolate, v8::Local<v8::Value> str) {
  v8::String::Utf8Value s(isolate, str);
  return *s;
}

/**
 * Error reporting utility based on ShellRunner::Run.
 * WARNING: It does not work very well. For some reason, we have to try/catch
 * inside the JS code to get a proper error message. Might have to do with the
 * fact that we are running this before the window and/or other mechanisms have
 * not fully initialized.
 */
static std::string GetStackTrace(v8::Isolate* isolate, v8::TryCatch& try_catch) {
  if (!try_catch.HasCaught()) {
    return "";
  }

  std::stringstream ss;
  v8::Local<v8::Message> message = try_catch.Message();
  if (!message.IsEmpty()) {
    ss << V8ToString(isolate, message->Get()) << std::endl;
  }
  ss << V8ToString(isolate, GetSourceLine(isolate, message)) << std::endl;

  // v8::Local<v8::StackTrace> trace = message->GetStackTrace();
  // if (trace.IsEmpty())
  //   return ss.str();

  // int len = trace->GetFrameCount();
  // for (int i = 0; i < len; ++i) {
  //   v8::Local<v8::StackFrame> frame = trace->GetFrame(isolate, i);
  //   ss << V8ToString(isolate, frame->GetScriptName()) << ":"
  //      << frame->GetLineNumber() << ":" << frame->GetColumn() << ": "
  //      << V8ToString(isolate, frame->GetFunctionName()) << std::endl;
  // }
  return ss.str();
}

static void RunScript(v8::Isolate* isolate, v8::Local<v8::Context> context, const char* source_raw, const char* filename) {
  v8::Local<v8::String> filename_string = ToV8String(isolate, filename);
  v8::ScriptOrigin origin(isolate, filename_string);

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::String> source = ToV8String(isolate, source_raw);
  auto maybe_script = v8::Script::Compile(context, source, &origin);

  v8::Local<v8::Script> script;
  if (!maybe_script.ToLocal(&script)) {
    recordreplay::Crash("Replay RunScript COMPILE failed: %s",
      GetStackTrace(isolate, try_catch).c_str());
  }
  v8::Local<v8::Value> rv;
  if (!script->Run(context).ToLocal(&rv)) {
    recordreplay::Crash("Replay RunScript INIT failed: %s",
      GetStackTrace(isolate, try_catch).c_str());
  }
}

static bool TestEnv(const char* env) {
  const char* v = getenv(env);
  return v && v[0] && v[0] != '0';
}

static void InitializeRecordReplayApiObjects(v8::Isolate* isolate, LocalFrame* localFrame) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  // Add __RECORD_REPLAY_ANNOTATION_HOOK__ as a global.
  SetFunctionProperty(isolate, context->Global(), AnnotationHookJSName,
                      InvokeOnAnnotation);

  v8::Local<v8::Object> jsrrApi = v8::Object::New(isolate);
  DefineProperty(isolate, context->Global(), "__RECORD_REPLAY__", jsrrApi);

  v8::Local<v8::Object> args = v8::Object::New(isolate);
  DefineProperty(isolate, context->Global(), "__RECORD_REPLAY_ARGUMENTS__",
                 args);

  DefineProperty(isolate, args, "REPLAY_CDT_PAUSE_OBJECT_GROUP",
                 ToV8String(isolate, REPLAY_CDT_PAUSE_OBJECT_GROUP));

  DefineProperty(
      isolate, args, "RECORD_REPLAY_DISABLE_SOURCEMAP_CACHE",
      v8::Boolean::New(isolate,
                       TestEnv("RECORD_REPLAY_DISABLE_SOURCEMAP_CACHE")));

  DefineProperty(isolate, args, "CDPERROR_MISSINGCONTEXT",
                 v8::Number::New(isolate, (double)CDPERROR_MISSINGCONTEXT));

  DefineProperty(isolate, args, "CDPERROR_NOTALIVE",
                 v8::Number::New(isolate, (double)CDPERROR_NOTALIVE));

  SetFunctionProperty(isolate, args, "log", LogCallback);
  SetFunctionProperty(isolate, args, "logTrace", LogTraceCallback);
  SetFunctionProperty(isolate, args, "warning", LogWarningCallback);

  // CDP debugger functionality
  SetFunctionProperty(isolate, args, "fromJsIsReplayScriptAlive",
                      fromJsIsReplayScriptAlive);
  SetFunctionProperty(isolate, args, "setCDPMessageCallback",
                      SetCDPMessageCallback);
  SetFunctionProperty(isolate, args, "sendCDPMessage", SendCDPMessage);
  SetFunctionProperty(isolate, args, "setCommandCallback",
                      v8::FunctionCallbackRecordReplaySetCommandCallback);

  // Object Util
  SetFunctionProperty(isolate, args, "fromJsMakeDebuggeeValue",
                      fromJsMakeDebuggeeValue);
  SetFunctionProperty(isolate, args, "fromJsGetArgumentsInFrame",
                      fromJsGetArgumentsInFrame);
  SetFunctionProperty(isolate, args, "fromJsGetObjectByCdpId",
                      fromJsGetObjectByCdpId);
  SetFunctionProperty(isolate, args, "fromJsIsBlinkObject",
                      fromJsIsBlinkObject);

  // networking
  SetFunctionProperty(isolate, args, "getCurrentNetworkRequestEvent",
                      GetCurrentNetworkRequestEvent);
  SetFunctionProperty(isolate, args, "getCurrentNetworkStreamData",
                      GetCurrentNetworkStreamData);

  // DOM, blink, API stuff
  // SetFunctionProperty(isolate, args, "jsGetObjectIdForAnyObject",
  //                     jsGetObjectIdForAnyObject);
  // SetFunctionProperty(isolate, args, "jsPreviewBlinkObjectForObjectId",
  // jsPreviewBlinkObjectForObjectId);
  SetFunctionProperty(isolate, args, "fromJsGetNodeIdByCpdId", fromJsGetNodeIdByCpdId);
  SetFunctionProperty(isolate, args, "fromJsGetBoxModel", fromJsGetBoxModel);
  SetFunctionProperty(isolate, args, "fromJsGetMatchedStylesForElement",
                      fromJsGetMatchedStylesForElement);
  SetFunctionProperty(isolate, args, "fromJsCssGetStylesheetByCpdId",
                      fromJsCssGetStylesheetByCpdId);
  SetFunctionProperty(isolate, args, "fromJsCollectEventListeners",
                      fromJsCollectEventListeners);
  SetFunctionProperty(isolate, args, "fromJsDomPerformSearch",
                      fromJsDomPerformSearch);
  SetFunctionProperty(isolate, args, "getFunctionBytecode",
                      fromJsGetFunctionBytecode);

  // Replay meta.
  DefineProperty(isolate, args, "IsReplaying",
                 v8::Boolean::New(isolate, recordreplay::IsReplaying()));
  SetFunctionProperty(isolate, args, "beginReplayCode",
                      fromJsBeginReplayCode);
  SetFunctionProperty(isolate, args, "endReplayCode",
                      fromJsEndReplayCode);

  // unsorted Replay stuff
  SetFunctionProperty(
      isolate, args, "setClearPauseDataCallback",
      v8::FunctionCallbackRecordReplaySetClearPauseDataCallback);
  SetFunctionProperty(isolate, args, "getCurrentError", GetCurrentError);
  SetFunctionProperty(isolate, args, "getRecordingId", GetRecordingId);
  SetFunctionProperty(isolate, args, "sha256DigestHex", SHA256DigestHex);
  SetFunctionProperty(isolate, args, "writeToRecordingDirectory",
                      WriteToRecordingDirectory);
  SetFunctionProperty(isolate, args, "addRecordingEvent", AddRecordingEvent);
  SetFunctionProperty(isolate, args, "addNewScriptHandler",
                      v8::FunctionCallbackRecordReplayAddNewScriptHandler);
  SetFunctionProperty(isolate, args, "getScriptSource",
                      v8::FunctionCallbackRecordReplayGetScriptSource);

  SetFunctionProperty(isolate, args, "recordingDirectoryFileExists",
                      RecordingDirectoryFileExists);
  SetFunctionProperty(isolate, args, "readFromRecordingDirectory",
                      ReadFromRecordingDirectory);
  SetFunctionProperty(isolate, args, "getRecordingFilePath",
                      GetRecordingFilePath);
  SetFunctionProperty(isolate, args, "getPersistentId", fromJsGetPersistentId);
  SetFunctionProperty(isolate, args, "checkPersistentId", fromJsCheckPersistentId);
}

void InitializeRecordReplay(v8::Isolate* isolate, LocalFrame* localFrame, v8::Local<v8::Context> context) {
  V8RecordReplaySetAPIObjectIdCallback(GetBlinkPersistentId);
  gActiveNetworkRequests =
      new std::unordered_map<std::string, NetworkRequestStatus>();
  gCurrentNetworkStreamData = new std::vector<uint8_t>();
}

void InitializeRecordReplayAfterCheckpoint() {
  // Note: This can immediately invoke the callback for events that happened
  // before the callback was registered.
  V8RecordReplayRegisterBrowserEventCallback(HandleBrowserEvent);
}

static void InitializeReplayScripts(v8::Isolate* isolate, LocalFrame* localFrame, v8::Local<v8::Context> context) {
  // Register context, s.t. when handling a command and we are not on a 
  // JS stack, we can always use the current root frame's context.
  // Note: We are assuming that each tab has its own process, for now.
  //   (That might not hold true for tabs of the same domain - not sure)
  V8RecordReplaySetDefaultContext(isolate, context);
  
  // Initialize __RECORD_REPLAY__ things.
  InitializeRecordReplayApiObjects(isolate, localFrame);

  // This URL will prevent the script from being reported to the recorder.
  const char* InternalScriptURL = "record-replay-internal";

  if (recordreplay::FeatureEnabled("collect-source-maps") &&
      !TestEnv("RECORD_REPLAY_DISABLE_SOURCEMAP_COLLECTION")) {
    recordreplay::AutoMarkReplayCode amrc;
    RunScript(isolate, context, gSourceMapScript, InternalScriptURL);
  }

  if (recordreplay::FeatureEnabled("force-main-world-initialization")) {
    // Call this here to avoid divergence later.
    // https://linear.app/replay/issue/RUN-2195#comment-e0b6c75b
    localFrame->GetSettings()->SetForceMainWorldInitialization(true);
  }

  if (IsGReplayScriptEnabled()) {
    recordreplay::AutoMarkReplayCode amrc;
    String commandHandlerScript = ReadReplayCommandHandlerScript();
    {
      recordreplay::AutoDisallowEvents disallow("InitializeReplayScripts");

      // Run `commandHandlerScript`.
      RunScript(isolate, context, commandHandlerScript.Utf8().c_str(), InternalScriptURL);
    }
  }
}

void OnRootFrameInit(v8::Isolate* isolate, LocalFrame* localFrame, v8::Local<v8::Context> context) {
  recordreplay::AutoMarkReplayCode amrc;
  recordreplay::Print(
    "[RUN-2739] OnRootFrameInit win=%d frame=%d %d \"%s\"",
      localFrame->DomWindow()->RecordReplayId(),
      localFrame->RecordReplayId(),
      localFrame->IsCrossOriginToParentOrOuterDocument(),
      localFrame->GetDocument()->Url().GetString().Utf8().c_str()
      );

  if (gReplayScriptsAlive) {
    // Our "V8RecordReplaySetDefaultContext" logic implies a single local
    // root frame per render process.
    recordreplay::Warning("ReplayScript Multiple_OnRootFrameInit");
    return;
  }
  
  // NOTE: The root `LocalFrame` can change over time.
  gRootLocalFrame = localFrame;

  // 1. Reset paint surface so that paints to the new root's surface are not ignored.
  // See: https://linear.app/replay/issue/RUN-2400
  recordreplay::DoResetPaintSurface();
  
  // 2. Initialize sourcemap worker, command handlers etc.
  gReplayScriptsAlive = true;
  recordreplay::Print("ReplayScript STATUS_CHANGE_ALIVE");
  InitializeReplayScripts(isolate, localFrame, context);
}

void OnRootFrameInitAfterCheckpoint(v8::Isolate* isolate, LocalFrame* localFrame, v8::Local<v8::Context> context) {
  // 1. Register navigation event.
  if (localFrame->GetDocument()->Url().ProtocolIsInHTTPFamily()) {
    recordreplay::OnNavigationEvent(
        nullptr, localFrame->GetDocument()->Url().GetString().Utf8().c_str());
  }

  // 2. Initialize React and Redux Devtools stubs.
  if (recordreplay::FeatureEnabled("react-devtools-backend") &&
      !TestEnv("RECORD_REPLAY_DISABLE_REACT_DEVTOOLS")) {
    // Note: We use a special URL for the react devtools as this script needs
    // to be reported to the recorder so that evaluations can be performed in
    // its frames.
    RunScript(isolate, context, gReactDevtoolsScript, "record-replay-react-devtools");
    RunScript(isolate, context, gReduxDevtoolsScript, "record-replay-redux-devtools");
  }
}

void OnNewWindowAfterCheckpoint(v8::Isolate* isolate, LocalFrame* localFrame, v8::Local<v8::Context> newContext) {
  recordreplay::AutoMarkReplayCode amrc;
  RunScript(isolate, newContext, gOnNewWindowScript,
            "record-replay-OnNewWindow");

  LocalFrame* parentFrame = DynamicTo<LocalFrame>(localFrame->Parent());
  recordreplay::Print(
    "[RUN-2739] OnNewWindowAfterCheckpoint %d win=%d frame=%d %d \"%s\" parent=%d",
    newContext == isolate->GetCurrentContext(),
    localFrame->DomWindow()->RecordReplayId(),
    localFrame->RecordReplayId(),
    localFrame->IsCrossOriginToParentOrOuterDocument(),
    localFrame->GetDocument()->Url().GetString().Utf8().c_str(),
    parentFrame ? parentFrame->RecordReplayId() : 0
  );
}


extern "C" void V8RecordReplayOnConsoleMessage(size_t bookmark);

static ErrorEvent* gCurrentErrorEvent;

void RecordReplayOnErrorEvent(ErrorEvent* error_event) {
  if (!v8::IsMainThread()) {
    return;
  }

  CHECK(!gCurrentErrorEvent);
  gCurrentErrorEvent = error_event;

  size_t bookmark = error_event->record_replay_bookmark();
  V8RecordReplayOnConsoleMessage(bookmark);

  gCurrentErrorEvent = nullptr;
}

static void GetCurrentError(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (!gCurrentErrorEvent) {
    return;
  }

  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Object> rv = v8::Object::New(isolate);

  SetDataProperty(isolate, rv, "message",
                  ToV8String(isolate, gCurrentErrorEvent->message().Utf8().c_str()));
  SetDataProperty(isolate, rv, "filename",
                  ToV8String(isolate, gCurrentErrorEvent->filename().Utf8().c_str()));
  SetDataProperty(isolate, rv, "line", v8::Number::New(isolate, gCurrentErrorEvent->lineno()));
  SetDataProperty(isolate, rv, "column", v8::Number::New(isolate, gCurrentErrorEvent->colno()));
  SetDataProperty(isolate, rv, "scriptId",
                  v8::Number::New(isolate, gCurrentErrorEvent->Location()->ScriptId()));

  args.GetReturnValue().Set(rv);
}

bool GetStringProperty(v8::Local<v8::Context> context, v8::Local<v8::Object> obj, const char* name, v8::Local<v8::String>* out) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::String> v8Name = ToV8String(isolate, name);
  v8::Local<v8::Value> v8Value = obj->Get(context, v8Name).ToLocalChecked();

  return v8Value->ToString(context).ToLocal(out);
}

bool GetObjectProperty(v8::Local<v8::Context> context, v8::Local<v8::Object> obj, const char* name, v8::Local<v8::Object>* out) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::String> v8Name = ToV8String(isolate, name);
  v8::Local<v8::Value> v8Value = obj->Get(context, v8Name).ToLocalChecked();

  return v8Value->ToObject(context).ToLocal(out);
}

bool StringEquals(v8::Isolate* isolate, v8::Local<v8::String> str1, const char* str2) {
  return str1->StringEquals(ToV8String(isolate, str2));
}

void RecordReplayEventListener::Invoke(ExecutionContext* context, Event* event) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Context> v8_context = isolate->GetCurrentContext();
  ScriptState* scriptState = ScriptState::Current(isolate);
  CustomEvent* customEvent = To<CustomEvent>(event);

  if (!customEvent) {
    return;
  }

  v8::Local<v8::Value> detail = customEvent->detail(scriptState).V8Value();
  v8::Local<v8::String> detail_json;
  if (!detail->ToString(v8_context).ToLocal(&detail_json)) {
    LOG(ERROR) << "[RUN-2863] RecordReplayEventListener: detail is not a string";
    return;
  }

  // for debugging:
  // LOG(ERROR) << "RecordReplayEventListener: detail = " << V8ToString(isolate, detail_json);

  // detail is a JSON stringified object with one of the following forms:

  // { "id": "record-replay-token", "message": { "type": "connect" } }      => register auth token observer
  // { "id": "record-replay-token", "message": { "type": "login" } }        => open external browser to login
  // { "id": "record-replay-token", "message": { "token": <string|null> } } => set access token if string.  clear if null (or undefined?)
  // { "id": "record-replay", "message": { "user": <string|null> } }        => set user if string.  clear if null (or undefined?)

  v8::Local<v8::Object> detail_obj;
  if (!v8::JSON::Parse(v8_context, detail_json).ToLocalChecked()->ToObject(v8_context).ToLocal(&detail_obj)) {
    LOG(ERROR) << "[RUN-2863] RecordReplayEventListener: detail is not a JSON object";
    return;
  }

  // always pull out the id and message properties, and early out if id isn't a string or message isn't an object
  v8::Local<v8::String> id_str;
  if (!GetStringProperty(v8_context, detail_obj, "id", &id_str)) {
    LOG(ERROR) << "[RUN-2863] RecordReplayEventListener: id is not an string";
    return;
  }

  v8::Local<v8::Object> message_obj;
  if (!GetObjectProperty(v8_context, detail_obj, "message", &message_obj)) {
    LOG(ERROR) << "[RUN-2863] RecordReplayEventListener: message is not an object";
    return;
  }


  if (StringEquals(isolate, id_str, "record-replay-token")) {
    HandleRecordReplayTokenMessage(v8_context, message_obj);
  } else if (StringEquals(isolate, id_str, "record-replay")) {
    HandleRecordReplayMessage(v8_context, message_obj);
  } else {
    LOG(ERROR) << "[RUN-2863] Unknown event id: " << V8ToString(isolate, id_str);
  }
}

void RecordReplayEventListener::HandleRecordReplayTokenMessage(v8::Local<v8::Context> context, v8::Local<v8::Object> message) {
  v8::Isolate* isolate = context->GetIsolate();

  // cases here:
  // { "id": "record-replay-token", "message": { "type": "connect" } }      => register auth token observer
  // { "id": "record-replay-token", "message": { "type": "login" } }        => open external browser to login
  // { "id": "record-replay-token", "message": { "token": <string|null> } } => set access token if string.  clear if null (or undefined?)

  // first check if there's a type property to handle the first two cases above.
  v8::Local<v8::Value> message_type = message->Get(context, ToV8String(isolate, "type")).ToLocalChecked();
  if (message_type->IsString()) {
    // message is either `{ type: "connect" }` or `{ type: "login" }`, with neither payload carrying additional info.
    if (StringEquals(isolate, message_type.As<v8::String>(), "connect")) {
      LOG(ERROR) << "[RUN-2863] RecordReplayEventListener: connect message received";
      local_frame_->RegisterRecordReplayAuthTokenObserver();
      return;
    }

    if (StringEquals(isolate, message_type.As<v8::String>(), "login")) {
      LOG(ERROR) << "[RUN-2863] RecordReplayEventListener: login message received";
      // [RUN-2863] TODO open external browser to login
      return;
    }

    LOG(ERROR) << "[RUN-2863] RecordReplayEventListener: unknown record-replay-token message type: " << V8ToString(isolate, message_type);
  }

  // if we're here, we should only be in the `{ token: ... }` case from the list above.
  v8::Local<v8::Value> message_token = message->Get(context, ToV8String(isolate, "token")).ToLocalChecked();
  if (message_token->IsString()) {
    LOG(ERROR) << "[RUN-2863] RecordReplayEventListener: set access token message received, token = " << V8ToString(isolate, message_token);
    // [RUN-2863] TODO set the access token in browser prefs.
    return;
  }

  if (message_token->IsNull()) {
    LOG(ERROR) << "[RUN-2863] RecordReplayEventListener: clear access token message received";
    // [RUN-2863] TODO clear the access token in browser prefs.
    return;
  }

  LOG(ERROR) << "[RUN-2863] RecordReplayEventListener: unknown record-replay-token message";
}

void RecordReplayEventListener::HandleRecordReplayMessage(v8::Local<v8::Context> context, v8::Local<v8::Object> message) {
  v8::Isolate* isolate = context->GetIsolate();

  // the only message handled here is `{ user: <string|null> }`
  v8::Local<v8::Value> message_user = message->Get(context, ToV8String(isolate, "user")).ToLocalChecked();
  if (message_user->IsString()) {
    LOG(ERROR) << "[RUN-2863] RecordReplayEventListener: set user message received, user = " << V8ToString(isolate, message_user);
    // [RUN-2863] TODO set the user in browser prefs.
    return;
  }

  if (message_user->IsNullOrUndefined()) {
    LOG(ERROR) << "[RUN-2863] RecordReplayEventListener: clear user message received";
    // [RUN-2863] TODO clear the user in browser prefs.
    return;
  }

  LOG(ERROR) << "[RUN-2863] Unknown record-replay message type";
  return;
}

void RecordReplayEventListener::Trace(Visitor* visitor) const {
  visitor->Trace(local_frame_);
  EventListener::Trace(visitor);
}

}  // namespace blink
