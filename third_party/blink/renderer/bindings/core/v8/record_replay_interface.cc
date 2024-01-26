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

static const char REPLAY_CDT_PAUSE_OBJECT_GROUP[] =
    "REPLAY_CDT_PAUSE_OBJECT_GROUP";


static bool IsGReplayScriptEnabled() {
  return recordreplay::IsReplaying() ||
         !recordreplay::FeatureEnabled("replay-only-gReplayScript");
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

// Script which defines handlers for recorder commands, and is only loaded while
// replaying.
const char* gReplayScript = R""""(
//js
(() => {

const EmptyArray = Object.freeze([]); // reduce unnecessary mem churn

const Verbose = false;
const VerboseCommands = Verbose;

const {
  log: log_,
  logTrace: logTrace_,
  warning: warning_,
  fromJsIsReplayScriptAlive: isReplayScriptAlive,
  setCDPMessageCallback,
  sendCDPMessage: sendCDPMessageRaw,
  setCommandCallback,
  setClearPauseDataCallback,
  addNewScriptHandler,
  getCurrentError,

  fromJsMakeDebuggeeValue,
  fromJsGetArgumentsInFrame,
  fromJsGetObjectByCdpId,
  fromJsIsBlinkObject,
  fromJsGetNodeIdByCpdId,
  fromJsGetBoxModel,
  fromJsGetMatchedStylesForElement,
  fromJsCssGetStylesheetByCpdId,
  fromJsCollectEventListeners,
  fromJsDomPerformSearch,

  // network
  getCurrentNetworkRequestEvent,
  getCurrentNetworkStreamData,

  // constants
  CDPERROR_MISSINGCONTEXT,
  CDPERROR_NOTALIVE,
  REPLAY_CDT_PAUSE_OBJECT_GROUP
} = __RECORD_REPLAY_ARGUMENTS__;

///////////////////////////////////////////////////////////////////////////////
// utils.js
///////////////////////////////////////////////////////////////////////////////

// Some of these are duplicated in gSourceMapScript, so watch out when making
// modifications to update both versions...

function isFunction(val) {
  return typeof val === "function";
}

function isObject(val) {
  return !!val && (typeof val === "object" || isFunction(val))
}

// eslint-disable-next-line no-unused-vars
function isNonNullObject(obj) {
  return obj && (typeof obj == "object" || typeof obj == "function");
}

function typeofMaybeNull(value) {
  if (value === null) {
    return "null";
  }
  return typeof value;
}

function log(...args) {
  log_(args.join(' '));
}

// eslint-disable-next-line no-unused-vars
function logTrace(...args) {
  logTrace_(args.join(' '));
}

function warning(...args) {
  warning_(args.join(' '));
}

function assert(v, msg = "") {
  if (!v) {
    const m = `Assertion failed when handling command (${msg})`;
    log(`[RuntimeError] ${m} - ${Error().stack}`);
    throw new Error(m);
  }
}

const gSourceMapData = new Map();

/** ###########################################################################
 * Use JS injection prevention:
 * Save some functions before User JS has a chance to overwrite them.
 * ##########################################################################*/

const JSON_stringify = JSON.stringify;
const JSON_parse = JSON.parse;

// RUN-3067
const Array_push = Array.prototype.push;

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
    log("[RuntimeError] Failed to process sourcemap url: " + err.message);
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

///////////////////////////////////////////////////////////////////////////////
// message.js
///////////////////////////////////////////////////////////////////////////////

function initMessages() {
  setCDPMessageCallback(messageCallback);
  setCommandCallback(commandCallback);
  setClearPauseDataCallback(clearPauseDataCallback);
}

let gNextMessageId = 1;

class CdpRequest {
  messageId;
  /**
   * CDP can send three possible types of results:
   *
   * 1. ProtocolError (id?, error: (code, message), data?)
   * @see https://github.com/replayio/chromium-v8/blob/c5e451943a6d87b44374e7a08d44fa92b9a2c93b/third_party/inspector_protocol/crdtp/dispatch.cc#L275
   *
   * 2. Response (id, result) - The response contains the return values defined by CDP.
   * @see https://github.com/replayio/chromium-v8/blob/c5e451943a6d87b44374e7a08d44fa92b9a2c93b/third_party/inspector_protocol/crdtp/dispatch.cc#L348
   *
   * 3. Notification (method, params) - TODO: we are not handling this yet.
   * @see https://github.com/replayio/chromium-v8/blob/c5e451943a6d87b44374e7a08d44fa92b9a2c93b/third_party/inspector_protocol/crdtp/dispatch.cc#L370
   */
  result;

  constructor(messageId) {
    this.messageId = messageId;
  }
}

const gCdpRequestStack = [];
const gEventListeners = new Map();


class CDPMessageError extends Error {
  constructor(message, code) {
    super(`${message} (${code})`);
    this.cdpMessage = message;
    this.code = code;
  }
}

function sendCDPMessage(method, params) {
  CHECK_ALIVE(`sendCDPMessage ${method}`);

  const id = gNextMessageId++;
  const cdpRequest = new CdpRequest(id);
  Array_push.call(gCdpRequestStack, cdpRequest);
  const cdpArgs = JSON_stringify({ method, params, id });
  try {
    sendCDPMessageRaw(cdpArgs);
  } catch (err) {
    if (!cdpRequest.result) {
      throw err;
    } else {
      // The CDP request was serviced, followed by a "ghostly" cross-origin
      // (and maybe other?) error:
      // Generally speaking, CDP commands should not throw.
      // If they do, we saw those errors being thrown by previous
      // user JS which happen to still be pending and then get thrown upon CDP
      // result return.
      // E.g.: https://linear.app/replay/issue/RUN-1680#comment-1dfa142b
      log(`[RuntimeError][RUN-1680] sendCDPMessage(${method}) failed: ${err?.message}`);
    }
  } finally {
    const req = gCdpRequestStack.pop();
    assert(req === cdpRequest, "[RuntimeError] CDP request stack corrupted");
  }

  if (cdpRequest.result?.result) {
    return cdpRequest.result.result;
  }
  if (cdpRequest.result?.error) {
    throw new CDPMessageError(cdpRequest.result.error.message, cdpRequest.result.error.code);
  }
  return undefined;
}

/**	
 * [RUN-3160] We have dependencies on this in the backend, via Target.evaluatePrivileged.	
 * @deprecated Use {@link sendCDPMessage} instead.	
 */	
// eslint-disable-next-line	
const sendMessage = sendCDPMessage;


function addEventListener(method, callback) {
  gEventListeners.set(method, callback);
}

// TODO: rename all these CDP-related symbols to also have CDP in the name
function messageCallback(message) {
  try {
    message = JSON_parse(message);
    if (message.id) {
      const request = gCdpRequestStack[gCdpRequestStack.length - 1];
      assert(message.id === request.messageId, "CDP request stack corrupted");
      request.result = message;
    } else {
      const listener = gEventListeners.get(message.method);
      if (listener) {
        listener(message.params);
      }
    }
  } catch (e) {
    warning(`JS Message callback exception: ${e?.stack || e}`);

    return JSON_stringify({
      is_error: true,
      message: e?.message || (e + ''),
      stack: e?.stack?.split?.("\n") || e?.stack || [],
      code: e?.code,
    });
  }
}

///////////////////////////////////////////////////////////////////////////////
// Command Handlers
///////////////////////////////////////////////////////////////////////////////

// Methods for interacting with the record/replay driver.

// Track all current execution contexts so that any scripts that we
// inject via evaluatePrivilegd can know what contexts are available.
const gExecutionContexts = new Map();
const gContextChangeCallbacks = new Set();

const CommandCallbacks = {
  "Graphics.getDevicePixelRatio": Graphics_getDevicePixelRatio,
  "Target.evaluatePrivileged": Target_evaluatePrivileged,
  "Target.getCurrentMessageContents": Target_getCurrentMessageContents,
  "Target.getSourceMapURL": Target_getSourceMapURL,
  "Target.getStepOffsets": Target_getStepOffsets,
  "Target.getCurrentNetworkRequestEvent": Target_getCurrentNetworkRequestEvent,
  "Target.getCurrentNetworkStreamData": Target_getCurrentNetworkStreamData,
  "Target.topFrameLocation": Target_topFrameLocation,
  "Pause.evaluateInFrame": Pause_evaluateInFrame,
  "Pause.evaluateInGlobal": Pause_evaluateInGlobal,
  "Pause.getAllFrames": Pause_getAllFrames,
  "Pause.getExceptionValue": Pause_getExceptionValue,
  "Pause.getObjectPreview": Pause_getObjectPreview,
  "Pause.getObjectProperty": Pause_getObjectProperty,
  "Pause.getScope": Pause_getScope,
  "DOM.getDocument": DOM_getDocument,
  "DOM.getAllBoundingClientRects": DOM_getAllBoundingClientRects,
  "DOM.getBoundingClientRect": DOM_getBoundingClientRect,
  "DOM.getBoxModel": DOM_getBoxModel,
  "DOM.getEventListeners": DOM_getEventListeners,
  "DOM.querySelector": DOM_querySelector,
  "DOM.performSearch": DOM_performSearch,
  "CSS.getComputedStyle": CSS_getComputedStyle,
  "CSS.getAppliedRules": CSS_getAppliedRules
};

function CHECK_ALIVE(message) {
  if (!isReplayScriptAlive()) {
    const err = new Error(`ReplayScript UNALIVE - ${message}`);
    err.code = CDPERROR_NOTALIVE;
    throw err;
  }
}

function getAliveLabel() {
  return isReplayScriptAlive() ? "" : " [UNALIVE]"
}

function executeCommand(method, params) {
  VerboseCommands && log(`[Command ${method}] Handling command, params=${JSON_stringify(params)}...`);
  const result = CommandCallbacks[method](params);
  VerboseCommands && log(`[Command ${method}] Handled command, result=${JSON_stringify(result)}`);
  return result;
}

function commandCallback(method, params) {
  if (!CommandCallbacks[method]) {
    log(`[RuntimeError][Command ${method}] Missing command callback: ${method}`);
    return {};
  }

  try {
    return executeCommand(method, params);
  } catch (e) {
    log(`[RuntimeError][Command ${method}]${getAliveLabel()} ${e?.stack || e}`);
    // Pass the error up to V8; it can (for now) decide how to handle itself, whether
    // it should crash or not, etc.  Eventually, the caller of the command should make
    // that decision.
    return {
      is_error: true,
      message: e?.message || (e + ''),
      stack: e?.stack?.split?.("\n") || e?.stack || [],
      code: e?.code,
    };
  }
}

function Target_evaluatePrivileged({ expression }) {
  const result = eval(expression);
  return { result };
}

const cdpToRrpConsoleLevels = new Map([
  ["info", "info"],
  ["warning", "warning"],
  ["error", "error"],
  ["timeEnd", "timeEnd"]
]);

// Contents of the last console API call. Runtime.consoleAPICalled will be
// emitted before the driver gets the current message contents.
let gLastConsoleAPICall;
function onConsoleAPICall(params) {
  gLastConsoleAPICall = params;
}

function Target_getCurrentMessageContents() {
  // We could be getting the contents of either an error object that was reported
  // to the driver via C++, or a console API call that was reported to the driver
  // via onConsoleAPICall(). We use these two paths because the bookmark
  // associated with thrown exceptions isn't available via the CDP currently.
  const error = getCurrentError();

  if (error) {
    const { message, filename, line, column, scriptId } = error;
    return {
      source: "PageError",
      level: "error",
      text: message,
      url: filename,
      sourceId: scriptId ? scriptId.toString() : undefined,
      line,
      column,
    };
  }

  if (!gLastConsoleAPICall) {
    return {
      source: "UnknownMessageError",
      level: "error",
      text: "Could not look up message contents"
    };
  }

  // Get the protocol representation of the message arguments.
  const argumentValues = [];
  for (const arg of gLastConsoleAPICall.args || []) {
    Array_push.call(argumentValues, buildRrpObjectFromCdpObject(arg));
  }

  const level = cdpToRrpConsoleLevels.get(gLastConsoleAPICall.type) || "info";

  let url, sourceId, line, column;
  if (gLastConsoleAPICall.stackTrace) {
    const frame = gLastConsoleAPICall.stackTrace.callFrames[0];
    if (frame) {
      url = frame.url;
      sourceId = frame.scriptId;
      line = frame.lineNumber;
      column = frame.columnNumber;
    }
  }

  return {
    source: "ConsoleAPI",
    level,
    text: "",
    url,
    sourceId,
    line,
    column,
    argumentValues,
  };
}

addNewScriptHandler((scriptId, sourceURL, relativeSourceMapURL) => {
  if (!relativeSourceMapURL)
    return;

  const urls = getSourceMapURLs(sourceURL, relativeSourceMapURL);
  if (!urls)
    return;

  const { sourceMapURL, sourceMapBaseURL } = urls;
  gSourceMapData.set(scriptId, {
    url: sourceMapURL,
    baseUrl: sourceMapBaseURL
  });
}, /* disallowEvents */ true);

function Target_getSourceMapURL({ sourceId }) {
  return gSourceMapData.get(sourceId) || {};
}

function Target_getStepOffsets() {
  // CDP does not distinguish between steps and breakpoints.
  return {};
}

function Target_getCurrentNetworkRequestEvent() {
  try {
    const obj = JSON_parse(getCurrentNetworkRequestEvent());
    return { data: obj };
  } catch (e) {
    warning(`JS Target.getCurrentNetworkRequestEvent exception: ${e}`);
  }
}

function Target_getCurrentNetworkStreamData(params) {
  const data = getCurrentNetworkStreamData(params);
  if (data) {
    return { data };
  } else {
    warning(`JS Target.getCurrentNetworkStreamData returned no data.`);
  }
}

function Target_topFrameLocation() {
  try {
    const { location } = sendCDPMessage("Debugger.getTopFrameLocation");
    if (!location) {
      return {};
    }
  } catch (e) {
    if (e instanceof CDPMessageError) {
      // No available context group; this can happen, so just return nothing.
      if (e.code == CDPERROR_MISSINGCONTEXT) {
        warning(`[RUN-2600] JS Target_topFrameLocation has no context.`);
        return {};
      }
    }
    throw e;
  }

  return { location: createProtocolLocation(location)[0] };
}

/**
 * Get the raw call frames on the stack, eliding ones in scripts we are ignoring.
 * @return {{ callFrames: CDP.Debugger.CallFrame[] }}
 *
 * @see https://chromedevtools.github.io/devtools-protocol/v8/Debugger/#type-CallFrame
 * @see https://github.com/replayio/chromium-v8/blob/37d50784b68747e7b2d5ebc16305cb9b3227741a/src/inspector/v8-debugger-agent-impl.cc#L1412
 */
function getStackFrames() {
  // NOTE: this is a custom command we added in `src/inspector/v8-debugger-agent-impl.cc`
  try {
    const { callFrames } = sendCDPMessage("Debugger.getCallFrames", {
      objectGroup: REPLAY_CDT_PAUSE_OBJECT_GROUP
    });
    return callFrames;
  } catch (e) {
    if (e instanceof CDPMessageError) {
      // No available context group; this can happen, so just return nothing.
      if (e.code == CDPERROR_MISSINGCONTEXT) {
        warning(`[RUN-2600] JS getStackFrames has no context.`);
        return [];
      }
    }
    throw e;
  }
}


// Build a protocol Result object from a result/exceptionDetails CDP rval.
function buildRrpObjectResult(cdpReturnValue) {
  const rrpResult = { data: {} };
  if (cdpReturnValue) {
    const { result: cdpResult, exceptionDetails } = cdpReturnValue;
    if (exceptionDetails) {
      /**
       * @see https://chromedevtools.github.io/devtools-protocol/tot/Runtime/#type-ExceptionDetails
       */
      const rrpObject = exceptionDetails.exception ?
        buildRrpObjectFromCdpObject(exceptionDetails.exception) :
        registerPlainObject({ message: exceptionDetails.text })
      rrpResult.exception = rrpObject;
    } else if (cdpResult) {
      // cdpResult is the actual result RemoteObject.
      const rrpObject = buildRrpObjectFromCdpObject(cdpResult);
      rrpResult.returned = rrpObject;
    }
  } else {
    // Sometimes things go wrong.
    // E.g. sometimes we get "Cannot find default execution context (-32000) when executing" sendCDPMessage
    // from Pause_evaluateIn*.
    log(`[RuntimeError] buildRrpObjectResult called without cdpReturnValue ()`);
    rrpResult.failed = true;
  }
  return { result: rrpResult };
}


function handleEvalError(err) {
  // RUN-2042 workaround: This fails a lot due to evals on frames in contexts
  // that have been destroyed. We want to fix this if we know that this is
  // high-impact.
  log(`[RuntimeError] in eval: ${err?.stack || err}`);
  return {
    failed: true
  };
}

let gCurrentEvaluateFrame;

/**
 * This queries all frames and returns the frame of given index.
 * @param {number} frameIndex
 */
function getFrameByIndex(frameIndex) {
  const frames = getStackFrames();
  assert(frameIndex >= 0 && frameIndex < frames.length, `Invalid frame index: ${frameIndex}`);
  return frames[frameIndex];
}

function getFrameByLocation(cdpLocation) {
  const frames = getStackFrames();
  return frames.find(
    f => JSON_stringify(f.location) == JSON_stringify(cdpLocation)
  );
}

/**
 * Returns the frame that `Pause.evaluateInFrame` was called on or undefined,
 * if not in the context of a `Pause.evaluateInFrame` call.
 */
function getCurrentEvaluateFrame() {
  return gCurrentEvaluateFrame;
}

function Pause_evaluateInFrame({ frameId: frameIndexStr, expression }) {
  const frameIndex = +frameIndexStr;
  const frame = getFrameByIndex(frameIndex);
  gCurrentEvaluateFrame = frame;
  let rv;
  try {
    onBeforeEval();
    rv = doEvaluation();
    return buildEvalResult(rv);
  } catch (err) {
    return handleEvalError(err);
  } finally {
    gCurrentEvaluateFrame = undefined;
  }

  function doEvaluation() {
    // In order to do the evaluation in the right frame, the same number of
    // frames need to be on V8's stack when we do the evaluation as when we got
    // the stack frames in the first place. The debugger agent extracts a frame
    // index from the ID it is given and uses that to walk the stack to the
    // frame where it will do the evaluation (see DebugStackTraceIterator).
    return sendCDPMessage(
      "Debugger.evaluateOnCallFrame",
      {
        callFrameId: frame.callFrameId,
        expression,
        objectGroup: REPLAY_CDT_PAUSE_OBJECT_GROUP
      }
    );
  }
}

function Pause_evaluateInGlobal({ expression }) {
  let rv;
  try {
    onBeforeEval();
    rv = sendCDPMessage(
      "Runtime.evaluate",
      {
        expression,
        objectGroup: REPLAY_CDT_PAUSE_OBJECT_GROUP
      }
    );
  } catch (err) {
    return handleEvalError(err);
  }
  return buildEvalResult(rv);
}

function onBeforeEval() {
  onReplayApiReset();
}

function buildEvalResult(cdpResult) {
  if (usedReplayApi && cdpResult?.exceptionDetails) {
    // Emit warning if an eval that used the Replay API throws.
    const cdpException = cdpResult.exceptionDetails.exception?.description || cdpResult.exceptionDetails;
    warning(`REPLAY_API_EVAL_ERROR ${JSON_stringify(cdpException)}`);
  }
  return buildRrpObjectResult(cdpResult);
}

function Pause_getAllFrames() {
  const frames = getStackFrames().map((frame, index) => {
    // Use our own IDs for frames.
    const id = (index++).toString();
    return createProtocolFrame(id, frame);
  });
  return {
    frames: frames.map(f => f.frameId),
    data: { frames },
  };
}

function Pause_getExceptionValue() {
  const rv = sendCDPMessage("Debugger.getPendingException", {
    objectGroup: REPLAY_CDT_PAUSE_OBJECT_GROUP
  });
  return { exception: rv.exception ? buildRrpObjectFromCdpObject(rv.exception) : undefined, data: {} };
}

function Pause_getObjectPreview({ object, level = "full", pageSizeForTesting = 0 }) {
  const objectData = createPauseObject(object, level, pageSizeForTesting);
  return { data: { objects: [objectData] } };
}

function Pause_getObjectProperty({ object, name }) {
  const cdpObj = getCdpObjectByRrpId(object);
  const rv = sendCDPMessage(
    "Runtime.callFunctionOn",
    {
      functionDeclaration: `function() { return this["${name}"] }`,
      objectId: cdpObj.objectId,
      objectGroup: REPLAY_CDT_PAUSE_OBJECT_GROUP
    }
  );
  return buildRrpObjectResult(rv);
}

function Pause_getScope({ scope }) {
  const scopeData = createRrpScope(scope);
  return { data: { scopes: [scopeData] } };
}

function Graphics_getDevicePixelRatio() {
  return { ratio: window?.devicePixelRatio || 0 };
}


///////////////////////////////////////////////////////////////////////////////
// Utilities
///////////////////////////////////////////////////////////////////////////////

function isPrototype(x) {
  // Note: This can invoke getters or proxy hooks on page objects,
  // so we watch for exceptions being thrown.
  try {
    return x === x?.constructor?.prototype;
  } catch (e) {
    return false;
  }
}

/**
 * Check whether given object `x` is a native object.
 */
function isBlinkObject(x) {
  return fromJsIsBlinkObject(x);
}

/**
 * Check whether given object `x` is a blink object and its
 * class name is `target.name`.
 * Note: This is a hackfix to work-around the fact that we have to be able
 * to deal with objects from multiple `global` contexts.
 */
function isBlinkInstanceOf(x, target) {
  return (
    // Is Blink/native object.
    isBlinkObject(x) &&
    // Is not a function (to exclude classes themselves).
    !isFunction(x) &&
    // Has a ctor.
    x.constructor &&
    // The target has a name.
    target?.name &&
    // The target's name is in object's prototype chain.
    hasInProtoChain(
      x.constructor,
      target.name
    )
  );
}

/**
 * This is kinda like `instanceof`, but window-independent.
 * NOTE: ideal solution is `x instanceof global[name]`, but we cannot do that since it does not work when dealing
 * with multiple instance of `global` (i.e. windows).
 * @see https://linear.app/replay/issue/RUN-1014/chromium-find-better-way-of-determining-dom-class-membership
 *
 * NOTE: `instanceof` is implemented in `Object::InstanceOf` -> `JSReceiver::HasInPrototypeChain`
 * @see https://github.com/replayio/gecko-dev/blob/592992ff7e15cb8ad1dd6fb109f19bd3523cd452/devtools/server/actors/replay/module.js#L1937
 * @see https://github.com/replayio/chromium-v8/blob/51140a440949dbbeea7a4e6c2185ccdeb8b6276e/src/objects/objects.cc#L929
 * @see https://github.com/replayio/chromium-v8/blob/51140a440949dbbeea7a4e6c2185ccdeb8b6276e/src/objects/js-objects.cc#170
 */
function hasInProtoChain(x, name) {
  if (x.name === name) {
    return true;
  }
  if (!x.__proto__) {
    return false;
  }
  return hasInProtoChain(x.__proto__, name);
}


///////////////////////////////////////////////////////////////////////////////
// object.js
// Manage association between remote objects and protocol object IDs.
///////////////////////////////////////////////////////////////////////////////


/**
 * This is mostly standard CDP `RemoteObject`s.
 * In some cases, CDP decided to have non-standard objects with
 * a separate id space (e.g. `CSSStylesheet`). We do not store those.
 *
 * @type {Map<string, CDP.Runtime.RemoteObject>}
 */
const gCdpObjectsByRrpId = new Map();

/**
 * @type {Map<string, string>}
 */
const gRrpIdByCdpId = new Map();
/**
 * @type {Map<Object, string>}
 */
const gRrpIdByPlainObject = new Map();
/**
 * @type {Map<string, Object>}
 */
const gPlainObjectByRrpId = new Map();

/**
 * Some preview objects are best constructed at an earlier time and then cached for
 * later use in this map.
 * @type {Map<string, Object>}
 */
const gObjectPreviewByRrpId = new Map();

let gLastRrpId = 0;

// Map protocol ObjectId => Debugger.Scope
// TODO: gCdpScopesByRrpId can probably be removed (use gCdpObjectsByRrpId instead)
const gCdpScopesByRrpId = new Map();

// cheap cache for boundingClientRects
const gLastBoundingClientRectsByNodeRrpId = new Map();

/**
 * @type {Map<>}
 */
const gCssRulesByNodeRrpId = new Map();

function clearPauseDataCallback() {
  try {
    gCdpObjectsByRrpId.clear();
    gRrpIdByCdpId.clear();
    gRrpIdByPlainObject.clear();
    gPlainObjectByRrpId.clear();
    gObjectPreviewByRrpId.clear();
    gCdpScopesByRrpId.clear();
    gLastBoundingClientRectsByNodeRrpId.clear();
    gCssRulesByNodeRrpId.clear();
    gLastRrpId = 0;

    // RUN-1832
    sendCDPMessage("Runtime.releaseObjectGroup", {
      objectGroup: REPLAY_CDT_PAUSE_OBJECT_GROUP,
    });
  } catch (e) {
    warning(`JS clearPauseDataCallback exception: ${e}`);
  }
}

/**
 * Creates and returns a new `CDP.RemoteObject` for given JS object.
 *
 * @return {CDP.Runtime.RemoteObject}
 * @see https://chromedevtools.github.io/devtools-protocol/tot/Runtime/#type-RemoteObject
 */
function makeDebuggeeValue(plainValue) {
  const remoteObject = fromJsMakeDebuggeeValue(plainValue);
  return remoteObject;
}

function createRrpValueRaw(plainValue) {
  const cdpObject = makeDebuggeeValue(plainValue);
  return buildRrpObjectFromCdpObject(cdpObject);
}

/**
 * @param {Object} plainObject
 * @return {number}
 */
function registerPlainObject(plainObject) {
  assert(isObject(plainObject),
    `value is not an object: ${typeofMaybeNull(plainObject)}`);
  let rrpId = gRrpIdByPlainObject.get(plainObject);
  if (!rrpId) {
    // → ask V8InspectorSession to wrap plainObject (gets CDP.Runtime.RemoteObject)
    const cdpObject = makeDebuggeeValue(plainObject);
    if (cdpObject) {
      rrpId = registerCdpObject(cdpObject);
      gRrpIdByPlainObject.set(plainObject, rrpId);
      gPlainObjectByRrpId.set(rrpId, plainObject);
    }
  }
  return rrpId;
}

function getPlainObjectByCdpId(cdpId) {
  const rrpId = gRrpIdByCdpId.get(cdpId);
  assert(rrpId);
  return getPlainObjectByRrpId(rrpId);
}

/**
 * @param {number} rrpId
 * @return {Object}
 */
function getPlainObjectByRrpId(rrpId) {
  rrpId += '';
  let plainObject = gPlainObjectByRrpId.get(rrpId);
  if (!plainObject) {
    // (if this was a ref type, registration should already have been handled in `registerCdpObject` ↓)
    // → ask V8InspectorSession to unwrap cdpObject (gets plainObject)
    const cdpObject = getCdpObjectByRrpId(rrpId);
    // → NOTE if we have an rrpId, it means, we already should have registered the cdpObject
    assert(cdpObject);
    const cdpId = cdpObject.objectId;
    plainObject = fromJsGetObjectByCdpId(cdpId);
    gRrpIdByPlainObject.set(plainObject, rrpId);
    gPlainObjectByRrpId.set(rrpId, plainObject);
  }
  return plainObject;
}

/**
 * @param {CDP.Runtime.RemoteObject}
 * @return {number} rrpId
 */
function registerCdpObject(cdpObject) {
  const cdpId = cdpObject.objectId;
  assert(cdpId);

  let rrpId = gRrpIdByCdpId.get(cdpId);
  if (rrpId) {
    return rrpId;
  }

  let plainObject;
  if (isCdpRefType(cdpObject)) {
    // NOTE: the same object might generate multiple cdpIds
    plainObject = fromJsGetObjectByCdpId(cdpId);
    if (plainObject) {
      rrpId = gRrpIdByPlainObject.get(plainObject);
    }
  }

  return registerNewRrpObject(rrpId, cdpObject, null, plainObject);
}


/**
 *
 * @return {CDP.Runtime.RemoteObject | Object}
 */
function getCdpObjectByRrpId(rrpId) {
  const cdpObject = gCdpObjectsByRrpId.get(rrpId);
  if (!cdpObject) {
    throw new Error(`getCdpObjectByRrpId failed - rrpId not found: ${JSON_stringify(rrpId)}`);
  }
  return cdpObject;
}

/**
 * Edge case: CDP calls produce custom objects that do NOT have an `objectId`.
 * Sometimes, they have their own id which refers back to some native plainObject
 *   (e.g. `CSSStylesheet`).
 * Sometimes they do not map to a native plainObject (e.g. `CSSRule`).
 * For such a CDP object, we only store its RRP preview extra and, for now, discard
 * its CDP representation.
 *
 *
 * @param {object} rrpObjectPreview Used in `getObjectPreview`.
 * @return {number} rrpId
 *
 * @see https://static.replay.io/protocol/tot/Pause/#type-ObjectPreview
 */
function registerRrpPreview(rrpObjectPreview, plainObject) {
  let rrpId;
  if (plainObject) {
    rrpId = gRrpIdByPlainObject.get(plainObject);
  }

  // NOTE: we built a custom "preview object" without a cdpObject, and sometimes without a plainObject
  const cdpObject = null;
  return registerNewRrpObject(rrpId, cdpObject, rrpObjectPreview, plainObject);
}

/**
 * Generates `rrpId`, if it does not have one yet.
 * Associates `rrpId` with its related data.
 */
function registerNewRrpObject(rrpId, cdpObject, rrpObjectPreview, plainObject) {
  // new RrpId
  const existingRrpId = rrpId;
  rrpId ||= ++gLastRrpId + '';  // coerce to string
  if (cdpObject) {
    // CDP.Runtime.RemoteObject
    assert(cdpObject.objectId);
    const cdpId = cdpObject.objectId;
    registerRrpCpdId(rrpId, cdpId, cdpObject);
  }
  if (rrpObjectPreview) {
    // preview objects, already built from specialized CDP objects
    gObjectPreviewByRrpId.set(rrpId, rrpObjectPreview);
    rrpObjectPreview.objectId = rrpId; // set `objectId`
  }
  if (plainObject && !existingRrpId) {
    gRrpIdByPlainObject.set(plainObject, rrpId);
    gPlainObjectByRrpId.set(rrpId, plainObject);
  }

  return rrpId;
}

function registerRrpCpdId(rrpId, cdpId, cdpObject = null) {
  gRrpIdByCdpId.set(cdpId, rrpId);
  if (cdpObject) {
    gCdpObjectsByRrpId.set(rrpId, cdpObject);
  }
}

/**
 * "Universal `arguments`" for any frame:
 * `arguments` are available by default in many function frames. However,
 * arrow functions do not have `arguments` available.
 * This function provides `arguments` for any type of frame.
 * @param {number | object | undefined} [frameOrFrameIndex] Optional frame argument. If none provided, pick the frame that the current Pause.evaluateInFrame call was requested for.
 * @see https://linear.app/replay/issue/RUN-1061#comment-fc1c3ee4
 * @see https://linear.app/replay/issue/RUN-2969/arrow-functions-the-arguments-keyword-and-chromium-vs-gecko#comment-989283b0
 */
function getFrameArgumentsArray(frameOrFrameIndex) {
  let frame;
  if (!frameOrFrameIndex) {
    if (!gCurrentEvaluateFrame) {
      throw new Error(`getFrameArgumentsArray must be called with a frame` +
        `object, frameIndex, or, if none provided, must be called from within ` +
        `the context of a Pause.evaluateInFrame call.`);
    }
    // Get new frame instance, since the stack might have changed and V8 uses
    // frame index for look up.
    frame = getFrameByLocation(gCurrentEvaluateFrame.location) 
    if (!frame) {
      throw new Error(
        `getFrameArgumentsArray was called from within Pause.evaluateInFrame ` +
        `but the frame is not on stack anymore: ${JSON_stringify(frames.map(f => f.location))}`);
    }
  } else if (typeof frameOrFrameIndex === "number") {
    frame = getFrameByIndex(frameOrFrameIndex);
  } else if (isObject(frameOrFrameIndex) && frameOrFrameIndex.callFrameId) {
    frame = frameOrFrameIndex;
  }
  const frameId = frame.callFrameId;
  const args = fromJsGetArgumentsInFrame(frameId);
  return args && [...args] || [];
}



// Strings longer than this will be truncated when creating protocol values.
// TODO This limit creates problems when we try to evaluate large strings in routines,
// such as stringifying a large object/array (like 2000+ unmounted fiber IDs).
// The RDT routine works around this by splitting the string into chunks, but
// we should find a better long-term solution (like bypassing the limit for evals).
const MaxStringLength = 10000;

const cdpRefTypes = ['object', 'function'];
function isCdpRefType(cdpObject) {
  return cdpRefTypes.includes(cdpObject.type);
}


/**
 *
 * @return {RRP.Pause.Object}
 */
function buildRrpObjectFromCdpObject(cdpObject) {
  if (!cdpObject) {
    return {};
  }
  switch (cdpObject.type) {
    case "undefined":
      return {};
    case "string":
    case "number":
    case "boolean":
      if (cdpObject.unserializableValue) {
        assert(cdpObject.type == "number");
        return { unserializableNumber: cdpObject.unserializableValue };
      }
      if (typeof cdpObject.value == "string" && cdpObject.value.length > MaxStringLength) {
        return { value: cdpObject.value.substring(0, MaxStringLength) + "…" };
      }
      return { value: cdpObject.value };
    case "bigint": {
      const str = cdpObject.unserializableValue;
      assert(str);
      return { bigint: str.substring(0, str.length - 1) };
    }
    case "object":
    case "function": {
      if (!cdpObject.objectId) {    // TODO: how can this happen?
        return { value: null };
      }

      const rrpId = registerCdpObject(cdpObject);
      return { object: rrpId };
    }
    case "symbol":
      return { symbol: cdpObject.description };
    default:
      log(`[RuntimeError] invalid CDP type: ${JSON_stringify(cdpObject)}`);
      return { unavailable: true };
  }
}


/**
 *
 * @param {CDP.Runtime.Scope} scope
 */
function registerCdpScope(scope) {
  const rrpId = registerCdpObject(scope.object);
  gCdpScopesByRrpId.set(rrpId, scope);
  return rrpId;
}

function getCdpScopeByRrpId(rrpScopeId) {
  const scope = gCdpScopesByRrpId.get(rrpScopeId);
  assert(scope);
  return scope;
}

function getBlinkNodeIdByRrpId(nodeRrpId) {
  const cdpObject = getCdpObjectByRrpId(nodeRrpId);
  const nodeId = fromJsGetNodeIdByCpdId(cdpObject.objectId);
  // Note: Don't generate assert message if assert did not fail.
  assert(nodeId, !nodeId && `${nodeRrpId}: ${JSON_stringify(cdpObject)}`);
  return nodeId;
}

///////////////////////////////////////////////////////////////////////////////
// preview.js
///////////////////////////////////////////////////////////////////////////////

// Logic for creating object previews for the record/replay protocol.

function isCdpObjectProxy(cdpObj) {
  return cdpObj.subtype === "proxy";
}

function isCdpObjectPromise(cdpObj) {
  return cdpObj.subtype === "promise";
}

/**
 * @return {RRP.Pause.Object}
 * @see https://static.replay.io/protocol/tot/Pause/#type-Object
 */
function createPauseObject(rrpId, level, pageSizeForTesting) {
  rrpId = rrpId + ""; // Must be a string.
  const existingPreview = gObjectPreviewByRrpId.get(rrpId);
  if (existingPreview) {
    return existingPreview;
  }

  const cdpObj = getCdpObjectByRrpId(rrpId);
  // NOTE: `subtype` is not reliably available, due to a divergence check in V8 → `value-mirror.cc`
  const className = isCdpObjectProxy(cdpObj) ? "Proxy" : (cdpObj.className || "Function");

  // NOTE: `persistentId` is added in V8 → `injected-script.cc`
  const { persistentId } = cdpObj;
  let preview;
  if (level != "none") {
    preview = new ProtocolObjectPreview(rrpId, cdpObj, level, pageSizeForTesting).fill();
  }

  return { objectId: rrpId, persistentId, className, preview };
}

// Return whether an object should be ignored when generating previews.
function isObjectBlacklisted(cdpObj) {
  // Accessing Storage object properties can cause hangs when trying to
  // communicate with the non-existent parent process.
  if (cdpObj.className == "Storage") {
    return true;
  }

  // Don't inspect scripted proxies, as we could end up calling into script.
  if (isCdpObjectProxy(cdpObj)) {
    return true;
  }

  return false;
}

// Return whether an object's property should be ignored when generating previews.
function isObjectPropertyBlacklisted(cdpObj, name) {
  if (isObjectBlacklisted(cdpObj)) {
    return true;
  }
  switch (name) {
    case "__proto__":
      // Accessing __proto__ doesn't cause problems, but is redundant with the
      // prototype reference included in the preview directly.
      return true;
  }
  return false;
}

// Target limit for the number of items (properties etc.) to include in object
// previews before overflowing.
const MaxItems = {
  "noProperties": 0,

  // Note: this is higher than on gecko-dev because typed arrays don't render
  // properly in the devtools currently unless we include a minimum number of
  // properties. This would be nice to fix.
  "canOverflow": 10,

  "full": 1000,
};

function ProtocolObjectPreview(rrpId, obj, level, pageSizeForTesting) {
  this.rrpId = rrpId;
  this.cdpObj = obj;
  this.level = level;
  this.pageSizeForTesting = pageSizeForTesting;
  this.overflow = false;
  this.numItems = 0;
  this.extra = {};
}

ProtocolObjectPreview.prototype = {
  get raw() {
    return this.plainObject;
  },

  get plainObject() {
    if (!this._plainObject) {
      this._plainObject = getPlainObjectByCdpId(this.cdpObj.objectId);
    }
    return this._plainObject;
  },

  startAddItem(force) {
    if (!force) {
      if (this.hasReachedItemLimit) {
        this.overflow = true;
        return false;
      }
      this.numItems++;
    }
    return true;
  },

  checkAddProperty(ownerCdpObject, name) {
    if (isObjectPropertyBlacklisted(ownerCdpObject, name)) {
      return false;
    }
    if (this.getterValues?.has(name)) {
      return false;
    }
    return true;
  },

  addProperty(ownerCdpObject, rrpProp, force) {
    if (this.checkAddProperty(ownerCdpObject, rrpProp.name)) {
      this.addPropertyUnchecked(rrpProp, force);
    }
  },

  addPropertyUnchecked(rrpProp, force) {
    if (!this.startAddItem(force)) {
      return;
    }
    if (!this.properties) {
      this.properties = [];
    }
    Array_push.call(this.properties, rrpProp);
  },

  addGetterValue(propKey, ownerCdpObject, force = false) {
    if (isObjectPropertyBlacklisted(ownerCdpObject, propKey)) {
      return;
    }

    if (!this.getterValues) {
      this.getterValues = new Map();
    }
    if (this.getterValues.has(propKey)) {
      return;
    }

    const rrpValue = evalPropRrpNotNull(this.raw, propKey);
    if (rrpValue) {
      this.setGetterValueUnchecked(propKey, rrpValue, force);
    }
  },

  addEvalMethodValue(propKey) {
    if (!this.getterValues) {
      this.getterValues = new Map();
    }
    if (this.getterValues.has(propKey)) {
      return;
    }

    const plainValue = this.raw[propKey].call(this.raw);
    const rrpValue = createRrpValueRaw(plainValue);
    if (rrpValue) {
      this.setGetterValueUnchecked(propKey, rrpValue, /* force */ true);
    }
    return plainValue;
  },

  setGetterValueUnchecked(key, valueObject, force = true) {
    if (!this.startAddItem(force)) {
      return;
    }
    if (!this.getterValues) {
      this.getterValues = new Map();
    }
    this.getterValues.set(key, { name: key, ...valueObject });
  },


  addContainerEntry(entry) {
    if (!this.startAddItem()) {
      return;
    }
    if (!this.containerEntries) {
      this.containerEntries = [];
    }
    Array_push.call(this.containerEntries, entry);
  },

  get unlimitedItems() {
    // Ignore prop limits of native objects.
    // (Because that is how we do it in gecko.)
    return isBlinkObject(this.raw, this.cdpObj);
  },

  get nRequestedItems() {
    return MaxItems[this.level] || 10;
  },

  get hasReachedItemLimit() {
    if (this.unlimitedItems) {
      return false;
    }
    return this.numItems >= this.nRequestedItems;
  },

  /**
   * Limit the amount of props we get back.
   * @see https://linear.app/replay/issue/RUN-1315/very-bad-command-performance-getallframes-wandb#comment-f8f54931
   */
  get pageSize() {
    // The +5 is a heuristic to force overflow.
    // We theoretically only want to add +1 but we might end up not getting
    // enough props to determine overflow, so +5 is slightly safer.
    // If +5 is not enough, we will loop and do a lot more work.
    if (this.pageSizeForTesting) {
      return this.pageSizeForTesting;
    }
    if (this.unlimitedItems) {
      return 0;
    }
    return this.nRequestedItems + 5;
  },

  /**
   * Ignore certain prototype props.
   */
  shouldAddProp(cdpProp) {
    // The debugger provides all prototype props on top of the object's own props.
    // This heuristic happens to keep only what we want:
    //    Own props and prototype getters.
    // See: https://linear.app/replay/issue/RUN-1592#comment-4011cec0
    return (cdpProp.isOwn || (cdpProp.configurable && cdpProp.enumerable)) &&
      this.checkAddProperty(this.cdpObj, cdpProp.name);
  },

  fill() {
    // Data returned from V8 debugger.
    let cdpProperties;

    if (this.pageSizeForTesting && !(this.pageSizeForTesting > 0)) {
      throw new Error("invalid pageSizeForTesting: " + this.pageSizeForTesting);
    }

    // Names of properties + getters.
    const foundProps = new Set();

    if (this.level === 'noProperties') {
      cdpProperties = { result: [] };
    } else {
      // Loop until we have as many items as requested. We need to loop because
      // the debugger, for some reason, also returns many unwanted prototype
      // props, which might wash out the actual props that we want.
      // See |shouldAddProp| for reference.
      let pageSize = 0;
      let nReturnedProperties = 0;

      do {
        // Note: Often, we get more properties than we asked for.
        pageSize = nReturnedProperties + this.pageSize;

        // WARNING: we manage possible divergences caused by `Runtime.getProperties` evaluating native getter
        //    in V8's |doesAttributeHaveObservableSideEffectOnGet|.
        //    see: https://github.com/replayio/chromium-v8/pull/115/files#diff-72ee0a91d32565577bd78ed94b034ae3b4bf51676c5d42165e9363cad18dccf9R1328
        try {
          cdpProperties = sendCDPMessage("Runtime.getProperties", {
            objectId: this.cdpObj.objectId,
            ownProperties: false,
            generatePreview: false,
            pageIndex: 0, // Warning: NYI
            pageSize,
            objectGroup: REPLAY_CDT_PAUSE_OBJECT_GROUP
          });
        } catch (e) {
          // No available context group; this can happen, so just return nothing.
          if (e.code == CDPERROR_MISSINGCONTEXT) {
            warning(`[RUN-2600] JS ProtocolObjectPreview.fill has no context.`);
            cdpProperties = { result: [] };
            break;
          } else {
            throw e;
          }
        }

        if (!cdpProperties.result) {
          return {
            prototypeId: undefined
          };
        }
        nReturnedProperties = cdpProperties.result.length;

        /**
         * @see https://chromedevtools.github.io/devtools-protocol/tot/Runtime/#type-PropertyDescriptor
         */
        for (let i = 0; i < cdpProperties.result.length; ++i) {
          const cdpProp = cdpProperties.result[i];
          const { name: propKey } = cdpProp;
          if (propKey === "__proto__" || foundProps.has(propKey)) {
            continue;
          }
          if (this.shouldAddProp(cdpProp)) {
            foundProps.add(propKey);
          }
        }

        // Keep going if we did not get enough items but the query returned as many items as requested.
        // Note: Go to +1 for the `overflow` flag.
        // log(`DDBG fill() C ${[foundProps.size, this.unlimitedItems, this.nRequestedItems, nReturnedProperties, pageSize].join(", ")}`);
      } while (
        !this.unlimitedItems &&
        foundProps.size <= (this.nRequestedItems + 1) &&
        nReturnedProperties >= pageSize
      );
    }
    
    for (const cdpProp of cdpProperties.result) {
      const { name: propKey } = cdpProp;
      if (!foundProps.has(propKey)) {
        continue;
      }
      const rrpProp = createRrpPropertyDescriptor(cdpProp);
      const force = false;
      this.addPropertyUnchecked(rrpProp, force);
    }

    /**
     * Explanation:The following logic depends on more `cdpProperties` but
     * is not affected by above `pageSize`:
     * 
     * 1. Inherent props of built-ins, such as Error.stack or Array.length are
     *    always added unconditionally.
     * 2. {Weak,}{Set,Map} is a container type whose contents are not in
     *    `properties`, but rather require a separate query that only returns
     *    actual container contents, thereby not requiring above loop.
     */

    // Add builtin-specific data.
    if (!isPrototype(this.raw)) { // Ignore prototype itself.
      const previewers = CustomPreviewers[this.cdpObj.className];
      if (previewers) {
        for (const entry of previewers) {
          if (isFunction(entry)) {
            entry.call(this, cdpProperties);
          } else {
            // entry should be string -> Look it up in results
            const cdpEntry = cdpProperties.result.find(prop => prop.name === entry);
            if (cdpEntry) {
              const rrpEntry = buildRrpObjectFromCdpObject(cdpEntry.value);
              this.setGetterValueUnchecked(entry, rrpEntry);
            }
          }
        }
      }
    }
    // Add data for blink and other special objects.
    Object.assign(this.extra, getExtraObjectPreviewData(this.cdpObj, cdpProperties));
    // Add Prototype data.
    let prototypeCdp = getInternalProp(cdpProperties, '[[Prototype]]')?.value;
    let prototypeRrpId;
    if (prototypeCdp) {
      prototypeRrpId = registerCdpObject(prototypeCdp);
    }

    // Produce final PauseData object.
    const result = {
      prototypeId: prototypeRrpId,
      overflow: (this.overflow && this.level != "full") ? true : undefined,
      properties: this.properties,
      getterValues: this.getterValues ? [...this.getterValues.values()] : undefined,
      containerEntries: this.containerEntries,
      ...this.extra,
    };

    return result;
  }
};

function getExtraObjectPreviewData(cdpObject, cdpProperties) {
  const cdpId = cdpObject.objectId;
  const rrpId = gRrpIdByCdpId.get(cdpId);
  assert(rrpId);
  
  if (isCdpObjectProxy(cdpObject)) {
    let targetCdpObj = getInternalProp(cdpProperties, '[[Target]]')?.value;
    let handlerCdpObj = getInternalProp(cdpProperties, '[[Handler]]')?.value;
    return {
      proxyState: {
        target: buildRrpObjectFromCdpObject(targetCdpObj),
        handler: buildRrpObjectFromCdpObject(handlerCdpObj)
      }
    };
  } else if (isCdpObjectPromise(cdpObject)) {
    let stateCdpObj = getInternalProp(cdpProperties, '[[PromiseState]]')?.value;
    let valueCdpObj = getInternalProp(cdpProperties, '[[PromiseResult]]')?.value;
    const promiseState = {
      state: stateCdpObj.value || undefined
    };
    if (promiseState.state !== "pending") {
      promiseState.value = buildRrpObjectFromCdpObject(valueCdpObj);
    }
    return {
      promiseState
    };
  } else {
    const plainObject = getPlainObjectByRrpId(rrpId);
    if (isBlinkInstanceOf(plainObject, Node)) {
      return {
        node: previewBlinkNode(plainObject)
      }
    }

    if (isBlinkInstanceOf(plainObject, CSSStyleDeclaration)) {
      return {
        style: previewBlinkStyle(plainObject)
      }
    }
  }
}

function previewBlinkNode(node) {
  let attributes, pseudoType;
  if (isBlinkInstanceOf(node, Element)) {
    attributes = [];
    for (const { name, value } of node.attributes || []) {
      Array_push.call(attributes, { name, value });
    }
    // TODO: We cannot access pseudo elements using the JS DOM API - https://linear.app/replay/issue/RUN-953/
    // pseudoType = node.localName;
  }

  let style;
  if (node.style) {
    style = registerPlainObject(node.style);
  }

  let parentNode;
  if (node.parentNode) {
    parentNode = registerPlainObject(node.parentNode);
  } else if (
    node.defaultView &&
    node.defaultView.parent != node.defaultView &&
    node.defaultView.parent?.document
  ) {
    /**
     * Nested documents use the parent element instead of null.
     *
     * TODO: will need more work here to support multi-CSP iframes
     *   (properly handle `iframe`s and the case where `node.defaultView.parent.document` is missing)
     *   Issue: https://linear.app/replay/issue/RUN-954/dom-feature-support-multi-cspcross-origin-iframes
     */
    const iframes = node.defaultView.parent.document.getElementsByTagName(
      "iframe"
    );
    const iframe = [...iframes].find((f) => f.contentDocument == node);
    if (iframe) {
      parentNode = registerPlainObject(iframe);
    }
  }

  let documentURL;
  if (node.nodeType == Node.DOCUMENT_NODE) {
    documentURL = node.URL;
  }

  const rv = {
    nodeType: node.nodeType,
    nodeName: node.nodeName,
    nodeValue: typeof node.nodeValue === "string" ? node.nodeValue : undefined,
    isConnected: node.isConnected,
    attributes,
    pseudoType,
    style,
    parentNode,
    documentURL,
  };
  

  let childNodes;
  if (node.nodeName == "IFRAME" && node.contentDocument) {
    // Treat an iframe's content document as one of its child nodes.
    childNodes = [registerPlainObject(node.contentDocument)];
  } else if (node.childNodes?.length) {
    childNodes = [...node.childNodes].map((n) => registerPlainObject(n));
  }

  if (childNodes) {
    rv.childNodes = childNodes;
  }

  return rv;
}

function previewBlinkStyle(style) {
  // NOTE: this is for inline styles, where there is no parentRule
  let parentRule = undefined;

  const properties = [];
  for (let i = 0; i < style.length; i++) {
    const name = style.item(i);
    const value = style.getPropertyValue(name);
    if (value) {
      const important = style.getPropertyPriority(name) == "important" ? true : undefined;
      Array_push.call(properties, { name, value, important });
    }
  }

  return {
    cssText: style.cssText,
    parentRule,
    properties
  };
}

function getDescriptionCount(description) {
  const match = /\((\d+)\)/.exec(description || "");
  if (match) {
    return +match[1];
  }
  return undefined;
}

function previewArray(_cdpProperties) {
  // TODO: [RUN-2223] Find out why Array.length does not always return a value.
  const length = getDescriptionCount(this.cdpObj.description);
  this.setGetterValueUnchecked("length", createRrpValueRaw(length));
}

function previewTypedArray() {
  // simply invoke the native getter
  this.addGetterValue('length', this.cdpObj, /* force */ true);
  this.addGetterValue('byteLength', this.cdpObj, /* force */ true);
  this.addGetterValue('byteOffset', this.cdpObj, /* force */ true);
  this.addGetterValue('buffer', this.cdpObj, /* force */ true);
}

/**
 * Query the internal object of {Weak,}{Set,Map}s that store their
 * containerEntries.
 */
function previewSetMap(cdpProperties) {
  if (!cdpProperties.internalProperties) {
    return;
  }

  const internal = getInternalProp(cdpProperties, "[[Entries]]");
  if (!internal || !internal.value || !internal.value.objectId) {
    return;
  }

  // Get size from description.
  let size;

  if (["Set", "Map"].includes(this.cdpObj.className)) {
    // NOTE: For some reason, the internal backing array size is capped to
    // pageSize for Set and Map.
    // This type of inconsistency is possible since *we* added paging to the
    // debugger (RUN-1315), and it might have (albeit small) negative impacts
    // like this.
    // SLN: Simply query the size getter instead.
    size = this.raw.size;
    const rrpSize = { name: "size", value: size };
    this.addPropertyUnchecked(rrpSize, /* force */ true);
    this.setGetterValueUnchecked(rrpSize.name, rrpSize, /* force */ true);
  } else {
    // Weak{Set,Map}
    size = getDescriptionCount(internal.value.description);
  }
  this.extra.containerEntryCount = size;

  const entries = sendCDPMessage("Runtime.getProperties", {
    objectId: internal.value.objectId,
    ownProperties: true,
    generatePreview: false,
    pageIndex: this.pageIndex,
    pageSize: this.pageSize,
    objectGroup: REPLAY_CDT_PAUSE_OBJECT_GROUP
  }).result;

  for (const entry of entries) {
    if (entry?.value?.subtype == "internal#entry") {
      const entryProperties = sendCDPMessage("Runtime.getProperties", {
        objectId: entry.value.objectId,
        ownProperties: true,
        generatePreview: false,
        objectGroup: REPLAY_CDT_PAUSE_OBJECT_GROUP
      }).result;
      const key = entryProperties.find(eprop => eprop.name == "key");
      const value = entryProperties.find(eprop => eprop.name == "value");
      if (value) {
        this.addContainerEntry({
          key: key ? buildRrpObjectFromCdpObject(key.value) : undefined,
          value: buildRrpObjectFromCdpObject(value.value),
        });
      }
    }
    if (this.overflow) {
      break;
    }
  }
}

function previewRegExp() {
  this.extra.regexpString = this.cdpObj.description;
}

function previewDate() {
  const dateTime = Date.parse(this.cdpObj.description);
  if (!Number.isNaN(dateTime)) {
    this.extra.dateTime = dateTime;
  }
}

function previewError() {
  this.setGetterValueUnchecked("name", { value: this.cdpObj.className });
}

const ErrorProperties = [
  "message",
  "stack",
  previewError,
];

function getInternalProp(cdpProperties, name) {
  return cdpProperties.internalProperties?.find(
    prop => prop.name == name
  );
}

function getInternalFunctionLocationProp(cdpProperties) {
  return getInternalProp(cdpProperties, '[[FunctionLocation]]');
}

function previewFunction(cdpProperties) {
  const nameProperty = cdpProperties.result.find(prop => prop.name == "name");
  const locationProperty = getInternalFunctionLocationProp(cdpProperties);

  if (nameProperty) {
    // RUN-1991: nameProperty.value might not always exist, for some reason.
    this.extra.functionName = nameProperty?.value?.value || "";
  }

  if (locationProperty) {
    const loc = locationProperty?.value?.value || "";
    if (!loc) {
      warning(`[RUN-1991] previewFunction missing location: ${JSON_stringify(nameProperty)}, ${JSON_stringify(locationProperty)}`);
    }
    this.extra.functionLocation = createProtocolLocation(loc);
  }
}



const CustomPreviewers = {
  Array: [previewArray],
  Int8Array: [previewTypedArray],
  Uint8Array: [previewTypedArray],
  Uint8ClampedArray: [previewTypedArray],
  Int16Array: [previewTypedArray],
  Uint16Array: [previewTypedArray],
  Int32Array: [previewTypedArray],
  Uint32Array: [previewTypedArray],
  Float32Array: [previewTypedArray],
  Float64Array: [previewTypedArray],
  BigInt64Array: [previewTypedArray],
  BigUint64Array: [previewTypedArray],
  Map: [previewSetMap],
  WeakMap: [previewSetMap],
  Set: [previewSetMap],
  WeakSet: [previewSetMap],
  RegExp: [previewRegExp],
  Date: [previewDate],
  Error: ErrorProperties,
  EvalError: ErrorProperties,
  RangeError: ErrorProperties,
  ReferenceError: ErrorProperties,
  SyntaxError: ErrorProperties,
  TypeError: ErrorProperties,
  URIError: ErrorProperties,
  Function: [previewFunction],
  AsyncFunction: [previewFunction],
};

/**
 * Get given prop from given object and get its value.
 * Return RRP wrapper.
 * Since we only use this for a small set of well defined
 * props, we emit a warning if the result is undefined or null.
 */
function evalPropRrpNotNull(owner, propKey) {
  try {
    const plainValue = owner[propKey];
    if (plainValue === undefined || plainValue === null) {
      // [RUN-2223] This should not happen.
      const e = new Error("");
      warning(`[RUN-2223] JS evalPropRrpNotNull got ${plainValue} when evaluating ${propKey} on ${typeof owner}, stack=${e.stack}`);
    }
    return createRrpValueRaw(plainValue);
  } catch (err) {
    warning(`JS evalPropRrpNotNull exception - calling ${propKey?.toString?.()} on ${typeof owner} - ${err?.stack || err}`);
    return null;
  }
}

function createRrpPropertyDescriptor(cdpProp) {
  // https://chromedevtools.github.io/devtools-protocol/tot/Runtime/#type-PropertyDescriptor
  const { name, value: cdpValue, writable, get, set, configurable, enumerable, symbol } = cdpProp;

  let rv = buildRrpObjectFromCdpObject(cdpValue);
  rv.name = name;

  let flags = 0;
  if (writable) {
    flags |= 1;
  }
  if (configurable) {
    flags |= 2;
  }
  if (enumerable) {
    flags |= 4;
  }
  if (flags != 7) {
    rv.flags = flags;
  }

  if (get && get.objectId) {
    rv.get = registerCdpObject(get);
  }
  if (set && set.objectId) {
    rv.set = registerCdpObject(set);
  }

  rv.isSymbol = !!symbol;

  return rv;
}

function createProtocolLocation(location) {
  if (!location) {
    return undefined;
  }
  const { scriptId, lineNumber, columnNumber } = location;
  return [{
    sourceId: scriptId,
    // CDP line numbers are 0-indexed, while RRP line numbers are 1-indexed.
    line: lineNumber + 1,
    column: columnNumber,
  }];
}

function createProtocolFrame(frameId, cdpFrame) {
  // CDP call frames don't provide detailed type information.
  const type = cdpFrame.functionName ? "call" : "global";

  return {
    frameId,
    type,
    functionName: cdpFrame.functionName || undefined,
    functionLocation: createProtocolLocation(cdpFrame.functionLocation),
    location: createProtocolLocation(cdpFrame.location),
    scopeChain: cdpFrame.scopeChain.map(registerCdpScope),
    this: buildRrpObjectFromCdpObject(cdpFrame.this),
  };
}

function createRrpScope(scopeId) {
  const cdpScope = getCdpScopeByRrpId(scopeId);

  let type;
  switch (cdpScope.type) {
    case "global":
      type = "global";
      break;
    case "with":
      type = "with";
      break;
    default:
      type = cdpScope.name ? "function" : "block";
      break;
  }

  let rrpId, bindings;
  if (type == "global" || type == "with") {
    rrpId = registerCdpObject(cdpScope.object);
  } else {
    bindings = [];

    const properties = sendCDPMessage("Runtime.getProperties", {
      objectId: cdpScope.object.objectId,
      ownProperties: true,
      generatePreview: false,
      objectGroup: REPLAY_CDT_PAUSE_OBJECT_GROUP
    }).result;
    for (const { name, value: cdpProp } of properties) {
      const rrpProp = buildRrpObjectFromCdpObject(cdpProp);
      Array_push.call(bindings, { ...rrpProp, name });
    }
  }

  return {
    scopeId,
    type,
    object: rrpId,
    functionName: cdpScope.name || undefined,
    bindings,
  };
}

/** ###########################################################################
 * {@link DOM_getDocument}
 * ##########################################################################*/
function DOM_getDocument() {
  const rrpId = registerPlainObject(window.document);

  return {
    data: {},
    document: rrpId
  };
}

/** ###########################################################################
 * {@link DOM_getAllBoundingClientRects}
 * ##########################################################################*/

function getLastBoundingClientRect(nodeRrpId) {
  return gLastBoundingClientRectsByNodeRrpId.get(nodeRrpId);
}

/**
 * @see https://static.replay.io/protocol/tot/DOM/#type-NodeBounds
 */
function DOM_getAllBoundingClientRects() {
  const cx = new StackingContext(window);
  cx.addChildren(window.document);

  const entries = cx.flatten();
  // Get elements in front-to-back order.
  entries.reverse();

  const elements = entries
    .map((elem, i) => {
      const id = registerPlainObject(elem.raw) || i;

      // Use the containing context of the element to find any
      // applicable transform for its offset.
      const transformMatrix =
        elem.containingContext.findAncestor(parent => !!parent.transformMatrix)
          ?.transformMatrix;

      // Offset the bounding client rect by the transform matrix
      // and containing iframe offset (if any).
      let { left, top, right, bottom } = shiftRect(
        elem.raw.getBoundingClientRect(),
        elem.offset,
        transformMatrix
      );

      // Get all client rects.
      const clientRects = [...elem.raw.getClientRects()].map(r => {
        const { left, top, right, bottom } =
          shiftRect(r, elem.offset, transformMatrix);
        return [left, top, right, bottom];
      });

      if (left >= right || top >= bottom) {
        return null;
      }

      const v = {
        node: id,
        rect: [left, top, right, bottom],
      };
      if (clientRects.length > 0) {
        v.rects = clientRects;
      }
      if (elem.style?.getPropertyValue("visibility") === "hidden") {
        v.visibility = "hidden";
      }
      if (elem.style?.getPropertyValue("pointer-events") === "none") {
        v.pointerEvents = "none";
      }

      gLastBoundingClientRectsByNodeRrpId.set(id, v);

      return v;
    })
    .filter((v) => !!v);

  return { elements };
};

/** ###########################################################################
 * {@link DOM_getBoundingClientRect}
 * ##########################################################################*/

/**
 * @see https://static.replay.io/protocol/tot/DOM/#type-BoxModel
 */
function DOM_getBoundingClientRect({ node }) {
  if (!gLastBoundingClientRectsByNodeRrpId.size) {
    // compute all basic bounding client rect sizes
    DOM_getAllBoundingClientRects();
  }
  const rects = getNodeBoundingClientRects(node);
  const rect = rects[0];

  return { rect };
}

/** ###########################################################################
 * {@link DOM_getBoxModel}
 * ##########################################################################*/

function getNodeBoundingClientRects(nodeRrpId) {
  const rectInfo = getLastBoundingClientRect(nodeRrpId);
  return rectInfo?.rects ||
    (rectInfo?.rect ?
      [rectInfo.rect] :
      [[0, 0, 20, 20]] // random default rect
    );
}

/**
 * @see https://static.replay.io/protocol/tot/DOM/#type-BoxModel
 */
function DOM_getBoxModel({ node: nodeRrpId }) {
  const nodeObj = getPlainObjectByRrpId(nodeRrpId);

  const model = {
    node: nodeRrpId
  };

  if (isBlinkInstanceOf(nodeObj, Element)) {
    const nodeId = getBlinkNodeIdByRrpId(nodeRrpId);
    /**
     * @see https://chromedevtools.github.io/devtools-protocol/tot/DOM/#type-BoxModel
     */
    const cdpModel = fromJsGetBoxModel(nodeId);

    if (cdpModel) {
      const {
        content, padding, border, margin,
        // width, height, shapeOutside
      } = cdpModel;
      Object.assign(
        model,
        {
          content,
          padding,
          border,
          margin
        }
      );
    }
  }

  if (!model.content) {
    // The given node does not have a box model.
    // -> Produce a "correct" output to prevent triggering of a session command
    // failure.
    // We do this because this is not technically a failure state. It makes
    // sense for the client to want to get a box model of a node no matter if it
    // has one or not.
    model.content = [];
    model.padding = [];
    model.border = [];
    model.margin = [];
  }

  return { model };
}


/** ###########################################################################
 * {@link DOM_getEventListeners}
 * ##########################################################################*/

function DOM_getEventListeners({ node }) {
  const nodeObject = getPlainObjectByRrpId(node);
  assert(nodeObject);

  const listenerInfos = fromJsCollectEventListeners(nodeObject);

  if (nodeObject.nodeName && nodeObject.nodeName == "HTML") {
    // Add event listeners for the document and window as well.
    Array_push.call(listenerInfos,
      ...fromJsCollectEventListeners(nodeObject.parentNode)   // document
      // ...fromJsCollectEventListeners(nodeObject.ownerGlobal)  // window
    );
  }

  const listeners = [];
  for (const { type, handler, capture } of listenerInfos) {
    if (!handler) {
      continue;
    }
    Array_push.call(listeners, {
      node,
      handler: registerPlainObject(handler),
      type,
      capture,
    });
  }

  return { listeners, data: {} };
}

/** ###########################################################################
 * {@link DOM_querySelector}
 * ##########################################################################*/

function DOM_querySelector({ node, selector }) {
  const nodeObj = getPlainObjectByRrpId(node);

  const resultObj = nodeObj.querySelector(selector);
  if (!resultObj) {
    return { data: {} };
  }
  const result = registerPlainObject(resultObj);
  return { result, data: {} };
}

/** ###########################################################################
 * {@link DOM_performSearch}
 * ##########################################################################*/

function DOM_performSearch({ query }) {
  query = query.trim();
  const nodeObjects = fromJsDomPerformSearch(query);
  const nodeRrpIds = nodeObjects
    ?.map(registerPlainObject)
    || [];

  return { nodes: nodeRrpIds, data: {} };
}


/** ###########################################################################
 * {@link CSS_getComputedStyle}
 * ##########################################################################*/

function CSS_getComputedStyle({ node }) {
  const nodeObj = getPlainObjectByRrpId(node);

  const computedStyle = [];
  if (isBlinkInstanceOf(nodeObj, Element)) {
    // NOTE: tested successfully for same-CSP elements of different iframes
    const ownerGlobal = window;

    // TODO: add pseudoType support - https://linear.app/replay/issue/RUN-953

    let styleInfo;
    // const pseudoType = getPseudoType(node);
    // if (pseudoType) {
    //   styleInfo = ownerGlobal.getComputedStyle(
    //     nodeObj.parentNode,
    //     pseudoType
    //   );
    // }
    // else {
    styleInfo = ownerGlobal.getComputedStyle(nodeObj);
    for (let i = 0; i < styleInfo.length; i++) {
      Array_push.call(computedStyle, {
        name: styleInfo.item(i),
        value: styleInfo.getPropertyValue(styleInfo.item(i)),
      });
    }
  }
  return { computedStyle };
}



/** ###########################################################################
 * {@link CSS_getAppliedRules}
 * ##########################################################################*/

/**
 *
 * @see https://developer.mozilla.org/en-US/docs/Web/API/CSSRule
 * @see https://developer.mozilla.org/en-US/docs/Web/API/CSSStyleRule
 * @see https://chromedevtools.github.io/devtools-protocol/tot/CSS/#type-CSSRule
 * @see https://static.replay.io/protocol/tot/CSS/#type-Rule
 */
function registerCdpAsRrpCssRule(nodeObj, cdpRule) {
  // NOTE: type is deprecated -> don't care
  const type = 1;
  let {
    selectorList = {},
    styleSheetId: styleSheetCpdId,
    style: {
      cssText: styleCssText,
      range: styleRange,
      cssProperties
    } = {},
    range: ruleRange,
    origin
  } = cdpRule || {};


  let styleSheetRrpId;
  if (styleSheetCpdId) {
    styleSheetRrpId = gRrpIdByCdpId.get(styleSheetCpdId);
    if (!styleSheetRrpId) {
      const nativeSheet = fromJsCssGetStylesheetByCpdId(styleSheetCpdId);

      // NOTE: `isSystem` is part of RRP from `gecko`.
      //    -> Chromium has a more diversified `StyleSheetOrigin` enum for this,
      //      (that is only accessible on the rule level in CDP, for some reason)
      const isSystem = origin !== 'regular';
      const styleSheet = { isSystem };
      if (nativeSheet?.href) {
        styleSheet.href = nativeSheet.href;
      }

      const styleSheetPreview = {
        className: 'RRPStyleSheetPreview', // no pre-defined className
        preview: {
          overflow: true,
          styleSheet
        }
      };
      styleSheetRrpId = registerRrpPreview(styleSheetPreview, nativeSheet);
      registerRrpCpdId(styleSheetRrpId, styleSheetCpdId);
    }
  }


  // stylePreview

  const properties = (cssProperties || [])
    .filter(prop => !!prop.text) // ignore props without text presentation
    .map(prop => {
      const { name, value, important } = prop;
      return {
        name,
        value,
        important
      };
    });
  /**
   * hackfix: for some reason, `user-agent` (and possibly other) styles don't have `cssText`.
   *    So, for now, we cook up a simple css serialization algo here.
   *    Native chromium has a better solution of course.
   * @see https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/css/style_property_serializer.cc;l=251;drc=3decef66bc4c08b142a19db9628e9efe68973e64
   * @see https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/css/style_property_serializer.cc;l=204;drc=3decef66bc4c08b142a19db9628e9efe68973e64
   */
  if (!styleCssText) {
    styleCssText = '\n  ' + properties
      .map(({ name, value, important }) => {
        const suffix = important ? ' !important' : '';
        return `${name}: ${value}${suffix};`;
      })
      .join('\n  ');
  }
  const stylePreview = {
    className: 'CSS2Properties', // `gecko` naming convention
    preview: {
      overflow: true,
      style: {
        cssText: styleCssText,
        parentRule: 0, // filled in once we have it, below
        properties
      }
    }
  };
  const nativeStlyeDeclaration = null;
  const styleRrpId = registerRrpPreview(stylePreview, nativeStlyeDeclaration);


  // rulePreview

  const startLine = (ruleRange || styleRange)?.startLine;
  const startColumn = (ruleRange || styleRange)?.startColumn;
  // see https://static.replay.io/protocol/tot/CSS/#type-OriginalStyleSheetLocation
  const originalLocation = undefined; // TODO
  const selectorText = selectorList?.text || '';

  /**
   * Based on `CSSStyleRule::cssText()`.
   * @see https://github.com/replayio/chromium/blob/052831f0220b79fe0c3343b49f6d2863ea6de05d/third_party/blink/renderer/core/css/css_style_rule.cc#L94
   */
  const ruleCssText = `${selectorText} {${styleCssText}}`;

  const rulePreview = {
    className: 'CSSRule',
    preview: {
      overflow: true,
      rule: {
        type,
        cssText: ruleCssText,
        parentStyleSheet: styleSheetRrpId,
        startLine,
        startColumn,
        originalLocation,
        selectorText,
        style: styleRrpId
      }
    }
  };

  // NOTE: we cannot currently lookup the native `CSSRule` object because
  //      InspectorCSSAgent::BuildObjectForRuleWithoutMedia does not
  //      store an id.
  // const nativeRule = lookupNativeCssRuleByCdpRule();
  const nativeRule = null;
  const ruleRrpId = registerRrpPreview(rulePreview, nativeRule);

  // set ruleRrpId
  stylePreview && (stylePreview.preview.style.parentRule = ruleRrpId);

  return ruleRrpId;
}


/**
 * NOTE1: RRP's `CSS.Rule` is based on how gecko does things.
 *    gecko has a utility function to produce the rules in one call.
 *    But in chromium, we have to query and convert the data in multiple steps.
 *
 *
 * @see https://chromedevtools.github.io/devtools-protocol/tot/CSS/#method-getMatchedStylesForNode
 *
 * @see https://linear.app/replay/issue/RUN-981/enhance-pausegetobjectpreview-css-previews
 * @see https://github.com/replayio/gecko-dev/blob/628cc55f22785f3a66a8c767cdc86f31feb9a050/layout/inspector/InspectorUtils.cpp#L155
 */
function convertCdpToRrpCssRules(nodeObj, cdpMatchedStyles) {
  const appliedRules = [];

  const {
    matchedRules = EmptyArray,
    inheritedEntries = EmptyArray,
    pseudoIdMatches = EmptyArray
  } = cdpMatchedStyles;

  function addCdpRule(cdpRule, pseudoElement = undefined) {
    const rrpRuleId = registerCdpAsRrpCssRule(nodeObj, cdpRule);
    const appliedRule = {
      rule: rrpRuleId,
      pseudoElement
    };
    Array_push.call(appliedRules, appliedRule);
  }

  for (const cdpRule of matchedRules) {
    addCdpRule(cdpRule.rule);
  }

  for (const cdpInheritedEntry of inheritedEntries) {
    // see https://chromedevtools.github.io/devtools-protocol/tot/CSS/#type-InheritedStyleEntry
    const {
      // inlineStyle, // inherited inline style
      matchedCSSRules  // inherited non-inline rules
    } = cdpInheritedEntry;

    for (const match of matchedCSSRules) {
      // match.matchingSelectors
      addCdpRule(match.rule);
    }
  }

  for (const pseudoMatch of pseudoIdMatches) {
    const {
      // see: https://chromedevtools.github.io/devtools-protocol/tot/DOM/#type-PseudoType
      pseudoType,
      // pseudoIdentifier,
      matches
    } = pseudoMatch;
    for (const match of matches) {
      addCdpRule(match.rule, pseudoType);
    }
  }

  return appliedRules;
}

function CSS_getAppliedRules({ node: nodeRrpId }) {
  const nodeObj = getPlainObjectByRrpId(nodeRrpId);

  let rules = gCssRulesByNodeRrpId.get(nodeRrpId);
  const data = {};

  if (!rules && isBlinkInstanceOf(nodeObj, Node)) {
    const nodeId = getBlinkNodeIdByRrpId(nodeRrpId);

    // NOTE: CDP CSS domain commands are not enabled, so we have to get the data indirectly.
    // const cdpMatchedStyles = sendCDPMessage('CSS.getMatchedStylesForNode', { nodeId });
    if (isBlinkInstanceOf(nodeObj, Element)) {
      const cdpMatchedStyles = fromJsGetMatchedStylesForElement(nodeId) || { };
      rules = convertCdpToRrpCssRules(nodeObj, cdpMatchedStyles);
    } else {
      rules = [];
    }
    gCssRulesByNodeRrpId.set(nodeRrpId, rules);
  } else {
    // The target is not a node.
    log(`[RuntimeWarning] CSS.getAppliedRules called with non-node: ${nodeRrpId} ${isBlinkObject(nodeObj)} ${nodeObj?.constructor?.name} ${nodeObj?.constructor}.`);
    rules = [];
  }

  return { rules, data };
}

/** ###########################################################################
 * StackingContext
 * ##########################################################################*/
// Mouse Targets Overview
//
// Mouse target data is used to figure out which element to highlight when the
// mouse is hovered/clicked on different parts of the screen when the element
// picker is used. To determine this, we need to know the bounding client rects
// of every element (easy) and the order in which different elements are stacked
// (not easy).
//
// To figure out the order in which elements are stacked, we reconstruct the
// stacking contexts on the page and the order in which elements are laid out
// within those stacking contexts, allowing us to assemble a sorted array of
// elements such that for any two elements that overlap, the frontmost element
// appears first in the array.
//
// References:
//
// https://www.w3.org/TR/CSS21/zindex.html
//
//   We try to follow this reference, although not all of its rules are
//   implemented yet.
//
// https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_Positioning/Understanding_z_index/The_stacking_context
//
//   This is helpful but the rules for when stacking contexts are created are
//   quite baroque and don't seem to match up with the spec above, so they are
//   mostly ignored here.

// Information about an element needed to add it to a stacking context.
function StackingContextElement(
  containingContext,
  node,
  parent,
  offset,
  style,
  clipBounds
) {
  // The stacking context this element is contained within.
  this.containingContext = containingContext;

  // Underlying element.
  this.raw = node;

  // Offset relative to the outer window of the window containing this context.
  this.offset = offset;

  // the parent StackingContextElement
  this.parent = parent;

  // Style and clipping information for the node.
  this.style = style;
  this.clipBounds = clipBounds;

  // Any stacking context at which this element is the root.
  this.context = null;
}

StackingContextElement.prototype = {
  isPositioned() {
    return this.style.getPropertyValue("position") != "static";
  },

  isAbsolutelyPositioned() {
    return ["absolute", "fixed"].includes(this.style.getPropertyValue("position"));
  },

  isTable() {
    return ["table", "inline-table"].includes(this.style.getPropertyValue("display"));
  },

  isFlexOrGridContainer() {
    return ["flex", "inline-flex", "grid", "inline-grid"].includes(
      this.style.getPropertyValue("display")
    );
  },

  isBlockElement() {
    return ["block", "table", "flex", "grid"].includes(this.style.getPropertyValue("display"));
  },

  isFloat() {
    return this.style.getPropertyValue("float") != "none";
  },

  getPositionedAncestor() {
    if (this.isPositioned()) {
      return this;
    }
    return this.parent?.getPositionedAncestor();
  },

  // see https://developer.mozilla.org/en-US/docs/Web/Guide/CSS/Block_formatting_context
  getFormattingContextElement() {
    if (!this.parent) {
      return this;
    }
    if (this.isFloat()) {
      return this;
    }
    if (this.isAbsolutelyPositioned()) {
      return this;
    }
    if (
      [
        "inline-block",
        "table-cell",
        "table-caption",
        "table",
        "table-row",
        "table-row-group",
        "table-header-group",
        "table-footer-group",
        "inline-table",
        "flow-root",
      ].includes(this.style.getPropertyValue("display"))
    ) {
      return this;
    }
    if (
      this.isBlockElement() &&
      !(
        ["visible", "clip"].includes(this.style.getPropertyValue("overflow-x")) &&
        ["visible", "clip"].includes(this.style.getPropertyValue("overflow-y"))
      )
    ) {
      return this;
    }
    if (["layout", "content", "paint"].includes(this.style.getPropertyValue("contain"))) {
      return this;
    }
    if (this.parent.isFlexOrGridContainer() && !this.isFlexOrGridContainer() && !this.isTable()) {
      return this;
    }
    if (
      this.style.getPropertyValue("column-count") != "auto" ||
      this.style.getPropertyValue("column-width") != "auto"
    ) {
      return this;
    }
    if (this.style.getPropertyValue("column-span") == "all") {
      return this;
    }
    return this.parent.getFormattingContextElement();
  },

  // toString() {
  //   return getObjectIdRaw(this.raw);
  // },
};

let gNextStackingContextId = 1;

// Information about all the nodes in the same stacking context.
// The spec says that some elements should be treated as if they
// "created a new stacking context, but any positioned descendants and
// descendants which actually create a new stacking context should be
// considered part of the parent stacking context, not this new one".
// For these elements we also create a StackingContext but pass the
// parent stacking context to the constructor as the "realStackingContext".
function StackingContext(window, options) {
  const {
    parentContext,
    root,
    offset,
    transformMatrix,
    realStackingContext
  } = options || {};
  this.window = window;
  this.parentContext = parentContext;
  this.id = gNextStackingContextId++;

  this.realStackingContext = realStackingContext || this;

  // Offset relative to the outer window of the window containing this context.
  this.offset = offset || { left: 0, top: 0 };

  // Transform scale parameter.  This is only relevant for stacking
  // contexts for IFRAME elements.
  if (transformMatrix) {
    assert(root && root.raw.tagName === "IFRAME");
  }
  this.transformMatrix = transformMatrix;

  // The arrays below are filled in tree order (preorder depth first traversal).

  // All non-positioned, non-floating elements.
  this.nonPositionedElements = [];

  // All floating elements.
  this.floatingElements = [];

  // All positioned elements with an auto or zero z-index.
  this.positionedElements = [];

  // Arrays of elements with non-zero z-indexes, indexed by that z-index.
  this.zIndexElements = new Map();

  this.root = root;
  if (root) {
    this.addChildrenWithParent(root);
  }
}

StackingContext.prototype = {
  toString() {
    return `StackingContext:${this.id}`;
  },

  // Find the first parent stacking context matching the predicate.
  findAncestor(predicate) {
    let cur = this;
    while (cur) {
      if (predicate(cur)) {
        return cur;
      }
      cur = cur.parentContext;
    }
    return null;
  },

  // Add node and its descendants to this stacking context.
  add(node, parentElem, offset) {
    const style = this.window.getComputedStyle(node);
    if (!style) {
      // It's not 100% clear why this is sometimes null, but it seems like
      // this can happen if DOM commands are sent when the window is shutting
      // down in some way or another.
      return;
    }

    const position = style.getPropertyValue("position");
    let clipBounds;
    if (position == "absolute") {
      clipBounds = parentElem?.getPositionedAncestor()?.clipBounds || {};
    } else if (position == "fixed") {
      clipBounds = {};
    } else {
      clipBounds = parentElem?.clipBounds || {};
    }
    clipBounds = Object.assign({}, clipBounds);
    const elem = new StackingContextElement(this, node, parentElem, offset, style, clipBounds);
    if (!["HTML", "BODY"].includes(elem.raw.tagName)) {
      if (style.getPropertyValue("overflow-x") != "visible") {
        const clipBounds2 = elem.getFormattingContextElement().raw.getBoundingClientRect();
        elem.clipBounds.left =
          clipBounds.left !== undefined
            ? Math.max(clipBounds2.left, clipBounds.left)
            : clipBounds2.left;
        elem.clipBounds.right =
          clipBounds.right !== undefined
            ? Math.min(clipBounds2.right, clipBounds.right)
            : clipBounds2.right;
      }
      if (style.getPropertyValue("overflow-y") != "visible") {
        const clipBounds2 = elem.getFormattingContextElement().raw.getBoundingClientRect();
        elem.clipBounds.top =
          clipBounds.top !== undefined
            ? Math.max(clipBounds2.top, clipBounds.top)
            : clipBounds2.top;
        elem.clipBounds.bottom =
          clipBounds.bottom !== undefined
            ? Math.min(clipBounds2.bottom, clipBounds.bottom)
            : clipBounds2.bottom;
      }
    }

    // Create a new stacking context for any iframes.
    if (elem.raw.tagName == "IFRAME" && elem.raw.contentWindow?.document) {
      let { left, top } = elem.raw.getBoundingClientRect();

      // The left and top are adjusted by the transform matrix for
      // the containing iframe, if any.  For this, we just search up the
      // context chain, looking for one with a defined transform matrix.
      let parentTransformMatrix =
        this.findAncestor(parent => !!parent.transformMatrix)
          ?.transformMatrix;
      if (parentTransformMatrix) {
        const adjusted = adjustCoordinateByTransformMatrix(
          [ left, top ],
          parentTransformMatrix
        );
        left = adjusted[0];
        top = adjusted[1];
      }

      // Compute the transform matrix for the iframe within its containing
      // document.
      // If we have a parent transform matrix, multiply it with this one.
      let transformMatrix = computeTransformMatrix(elem.raw, this.window);
      if (parentTransformMatrix) {
        transformMatrix = multiplyTransformMatrix(
          parentTransformMatrix,
          transformMatrix
        );
      }

      this.addContext(elem, undefined, { left, top, transformMatrix });
      elem.context.addChildren(elem.raw.contentWindow.document);
    }

    if (!elem.style) {
      this.addNonPositionedElement(elem);
      this.addChildrenWithParent(elem);
      return;
    }

    const parentDisplay = elem.parent?.style?.getPropertyValue("display");
    if (
      position != "static" ||
      ["flex", "inline-flex", "grid", "inline-grid"].includes(parentDisplay)
    ) {
      const zIndex = elem.style.getPropertyValue("z-index");
      if (zIndex != "auto") {
        this.addContext(elem, undefined, {});
        // Elements with a zero z-index have their own stacking context but are
        // grouped with other positioned children with an auto z-index.
        const index = +zIndex | 0;
        if (index) {
          this.realStackingContext.addZIndexElement(elem, index);
          return;
        }
      }

      if (position != "static") {
        this.realStackingContext.addPositionedElement(elem);
        if (!elem.context) {
          this.addContext(elem, this.realStackingContext, {});
        }
      } else {
        this.addNonPositionedElement(elem);
        if (!elem.context) {
          this.addChildrenWithParent(elem);
        }
      }
      return;
    }

    if (elem.isFloat()) {
      // Group the element and its descendants.
      this.addContext(elem, this.realStackingContext, {});
      this.addFloatingElement(elem);
      return;
    }

    const display = elem.style.getPropertyValue("display");
    if (display == "inline-block" || display == "inline-table") {
      // Group the element and its descendants.
      this.addContext(elem, this.realStackingContext, {});
      this.addNonPositionedElement(elem);
      return;
    }

    // Handle opacity-based stacking context creation _after_
    // we check for positioned elements.
    // This is on the assumption that the rules for floating and
    // positioned elements should apply before the rules for opacity.

    // Elements with `opacity < 1` get their own stacking context.
    let opacity = 1;
    const opacityStr = elem.style.getPropertyValue("opacity");
    if (opacityStr !== undefined && opacityStr !== "") {
      opacity = +opacityStr;
    }
    if (opacity < 1) {
      this.addContext(elem, undefined, {});
    }

    this.addNonPositionedElement(elem);
    this.addChildrenWithParent(elem);
  },

  addContext(
    elem,
    realStackingContext,
    { left, top, transformMatrix } = {}
  ) {
    if (elem.context) {
      assert(!left && !top, "!left && !top");
      return;
    }

    left = left || 0;
    top = top || 0;

    const offset = {
      left: this.offset.left + left,
      top: this.offset.top + top,
    };
    elem.context = new StackingContext(this.window, {
      parentContext: this,
      root: elem,
      offset,
      realStackingContext,
      transformMatrix
    });
  },

  addZIndexElement(elem, index) {
    const existing = this.zIndexElements.get(index);
    if (existing) {
      Array_push.call(existing, elem);
    } else {
      this.zIndexElements.set(index, [elem]);
    }
  },

  addPositionedElement(elem) {
    Array_push.call(this.positionedElements, elem);
  },

  addFloatingElement(elem) {
    Array_push.call(this.floatingElements, elem);
  },

  addNonPositionedElement(elem) {
    Array_push.call(this.nonPositionedElements, elem);
  },

  addChildren(parentNode) {
    for (const child of parentNode.children) {
      if (!isBlinkInstanceOf(child, Element)) {
        continue;
      }
      this.add(child, undefined, this.offset);
    }
  },

  addChildrenWithParent(parentElem) {
    for (const child of parentElem.raw.children) {
      if (!isBlinkInstanceOf(child, Element)) {
        continue;
      }
      this.add(child, parentElem, this.offset);
    }
  },

  // Get the elements in this context ordered back-to-front.
  flatten() {
    const rv = [];

    const pushElements = (elems) => {
      for (const elem of elems) {
        if (elem.context && elem.context != this) {
          Array_push.call(rv, ...elem.context.flatten());
        } else {
          Array_push.call(rv, elem);
        }
      }
    };

    const pushZIndexElements = (filter) => {
      for (const z of zIndexes) {
        if (filter(z)) {
          pushElements(this.zIndexElements.get(z));
        }
      }
    };

    const zIndexes = [...this.zIndexElements.keys()];
    zIndexes.sort((a, b) => a - b);

    if (this.root) {
      pushElements([this.root]);
    }
    pushZIndexElements((z) => z < 0);
    pushElements(this.nonPositionedElements);
    pushElements(this.floatingElements);
    pushElements(this.positionedElements);
    pushZIndexElements((z) => z > 0);

    return rv;
  },
};

/** ###########################################################################
 * {@link shiftRect}
 * ##########################################################################*/
function shiftRect(rect, offset, transformMatrix) {
  // Apply the transform to the rect, before offsetting it.
  // The offset has already been adjusted as needed by any transforms
  // that apply to it.
  let { left, top, right, bottom } = rect;
  if (transformMatrix) {
    if (left && top) {
      const [ leftTrans, topTrans ] = adjustCoordinateByTransformMatrix(
        [ left, top ],
        transformMatrix
      );
      left = leftTrans;
      top = topTrans;
    }
    if (right && bottom) {
      const [ rightTrans, bottomTrans ] = adjustCoordinateByTransformMatrix(
        [ right, bottom ],
        transformMatrix
      );
      right = rightTrans;
      bottom = bottomTrans;
    }
  }

  left = rect.left !== undefined ?  offset.left + left : undefined;
  top = rect.top !== undefined ?  offset.top + top : undefined;
  right = rect.right !== undefined ?  offset.left + right : undefined;
  bottom = rect.bottom !== undefined ?  offset.top + bottom : undefined;
  return { left, top, right, bottom };
}

/** ###########################################################################
 * {@link parseCssTransformStringToMatrix}
 * Parses a CSS transform string into an 6-element array representing a 2D
 * transformation matrix.
 * ```
 * [ scaleX, skewX, skewY, scaleY, translateX, translateY ]
 * ```
 * On error, returns undefined.
 * ##########################################################################*/
function parseCssTransformStringToMatrix(transform) {
  if (!transform || transform === "none") {
    return;
  }
  try {
    // see https://developer.mozilla.org/en-US/docs/Web/API/CSSStyleValue/parse_static
    const parsedTransform = CSSStyleValue.parse(
      "transform",
      transform,
    );
    // FIXME: We only handle 2D transforms for now.
    if (!parsedTransform.is2D) {
      return;
    }
    if (parsedTransform.length > 0) {
      const { a, b, c, d, e, f } = parsedTransform[0].toMatrix();
      return [a, b, c, d, e, f];
    }
  } catch (err) {
    // FIXME: log a command diagnostic / warning.
  }
}

/** ###########################################################################
 * {@link computeTransformMatrix}
 * Compute the full transform for an element within its containing document.
 * ##########################################################################*/
function computeTransformMatrix(element, window) {
  let curMatrix = [1,0,0,1,0,0]; // start with identity matrix
  let curElem = element;
  while(curElem && isBlinkInstanceOf(curElem, Element)) {
    const transformStr = window.getComputedStyle(curElem).transform;
    const transformMatrix = parseCssTransformStringToMatrix(transformStr);
    if (transformMatrix) {
      curMatrix = multiplyTransformMatrix(transformMatrix, curMatrix);
    }
    curElem = curElem.parentNode;
  }
  return curMatrix;
}

/** ###########################################################################
 * {@link multiplyTransformMatrix}
 * Multiply two transform matrices of teh form [a, b, c, d, tx, ty]
 * ##########################################################################*/
function multiplyTransformMatrix(m1,m2) {
  // [a, b, c, d, tx, ty] => [
  //   a, c, tx,
  //   b, d, ty,
  //   0, 0, 1
  // ]
  const [a1, b1, c1, d1, tx1, ty1] = m1;
  const [a2, b2, c2, d2, tx2, ty2] = m2;

  const a3 = a1 * a2 + c1 * b2;
  const b3 = b1 * a2 + d1 * b2;
  const c3 = a1 * c2 + c1 * d2;
  const d3 = b1 * c2 + d1 * d2;
  const tx3 = a1 * tx2 + c1 * ty2 + tx1;
  const ty3 = b1 * tx2 + d1 * ty2 + ty1;
  return [a3, b3, c3, d3, tx3, ty3];
}

/** ###########################################################################
 * {@link adjustCoordinateByTransformMatrix}
 * Adjust a { left, top } coordinate by a transform matrix
 * ##########################################################################*/
function adjustCoordinateByTransformMatrix(coord, m) {
  const [ x, y ] = coord;
  const [scaleX, skewX, skewY, scaleY, translateX, translateY] = m;

  const x2 = x * scaleX + y * skewX + translateX;
  const y2 = x * skewY + y * scaleY + translateY;
  return [x2, y2];
}

/** ###########################################################################
 * Export internal methods via `__RECORD_REPLAY_ARGUMENTS__`.
 * This is to be used for internal debugging purposes.
 * ##########################################################################*/

// TODO: Get rid of `internal`. There is no reason to have this in addition to
//       __RECORD_REPLAY_ARGUMENTS__ and __RECORD_REPLAY__.
__RECORD_REPLAY_ARGUMENTS__.internal = {
  getBlinkNodeIdByRrpId,
  getCdpObjectByRrpId,
  fromJsGetNodeIdByCpdId,
  getPlainObjectByRrpId,
  registerPlainObject,
  gLastBoundingClientRectsByNodeRrpId,
  sendCDPMessage,
  getNextStackingContextId: () => gNextStackingContextId,
  setNextStackingContextId: (id) => { gNextStackingContextId = id; },
  updateNextStackingContextId: (f) => { gNextStackingContextId = f(gNextStackingContextId); },
};

/** ###########################################################################
 * {@link replayEval}
 * ##########################################################################*/

/**
 * Execute a function but only when replaying and with events disallowed.
 */
function replayEval(fn) {
  const {
    beginReplayCode,
    endReplayCode
  } = __RECORD_REPLAY_ARGUMENTS__;
  beginReplayCode("replayEval");
  try {
    // We cannot currently avoid a user-supplied function from getting
    // instrumented. Stringifying and evaling it with events disallowed
    // fixes that problem.
    let fnExpr = fn.toString().trim();
    eval(`(${fnExpr})()`);
  } catch (err) {
    // Note: We MUST NOT let this error escape, or it will cause a mismatch in
    // `Runtime_UnwindAndFindExceptionHandler`.
    // TODO: We should just crash here, since its the responsibility of the
    // caller of replayEval to make sure the cb won't throw.
    warning(`replayEval ERROR: ${err?.stack || err}`);
  } finally {
    endReplayCode();
  }
}

/** ###########################################################################
 * Export JS API methods via `__RECORD_REPLAY__`.
 * This is officially available for scripts in `eval*` commands to use.
 * ##########################################################################*/
Object.assign(__RECORD_REPLAY__, {
  getProtocolIdForObject(obj) {
    return registerPlainObject(obj);
  },
  getObjectFromProtocolId(rrpId) {
    return getPlainObjectByRrpId(rrpId);
  },
  executeCommand,
  log,
  warning,
  getFrameArgumentsArray,
  getCurrentEvaluateFrame,
  replayEval
});

/** ###########################################################################
 * {@link patchReplayApi} decorates our API objects/functions with extra
 * diagnostics, e.g. whether the Replay API was called at all.
 * ##########################################################################*/

let usedReplayApi = 0;

function onReplayApiUsed() {
  ++usedReplayApi;
}

function onReplayApiReset() {
  usedReplayApi = 0;
}

function patchReplayApi() {
  patchReplayApiObject(__RECORD_REPLAY__);
  patchReplayApiObject(__RECORD_REPLAY_ARGUMENTS__);
  patchReplayApiObject(__RECORD_REPLAY_ARGUMENTS__.internal);
}

function patchReplayApiObject(obj) {
  for (const key in obj) {
    const value = obj[key];
    if (isFunction(value)) {
      obj[key] = wrapReplayApiFunction(value);
    }
  }
}

function wrapReplayApiFunction(fn) {
  return (...args) => {
    onReplayApiUsed();
    return fn(...args);
  };
}


///////////////////////////////////////////////////////////////////////////////
// main.js
///////////////////////////////////////////////////////////////////////////////

patchReplayApi();
initMessages();
addEventListener("Runtime.consoleAPICalled", onConsoleAPICall);
addEventListener("Runtime.executionContextCreated", ({ context }) => {
  gExecutionContexts.set(context.id, context);
  for (const callback of gContextChangeCallbacks) {
    callback(context, "add");
  }
});
addEventListener("Runtime.executionContextDestroyed", ({ executionContextId }) => {
  const context = gExecutionContexts.get(executionContextId);
  for (const callback of gContextChangeCallbacks) {
    callback(context, "remove");
  }
  gExecutionContexts.delete(executionContextId);
});
addEventListener("Runtime.executionContextsCleared", () => {
  for (const context of gExecutionContexts.values()) {
    for (const callback of gContextChangeCallbacks) {
      callback(context, "remove");
    }
  }
  gExecutionContexts.clear();
});
sendCDPMessage("Runtime.enable");

})();

)"""";







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
  try {
    window.__REACT_DEVTOOLS_GLOBAL_HOOK__ = window.top.__REACT_DEVTOOLS_GLOBAL_HOOK__;
    window.__REDUX_DEVTOOLS_EXTENSION__ = window.top.__REDUX_DEVTOOLS_EXTENSION__;
    window.__REDUX_DEVTOOLS_EXTENSION_COMPOSE__ = window.top.__REDUX_DEVTOOLS_EXTENSION_COMPOSE__;

    // TODO: Feels like this cross-context function usage can cause trouble, especially when
    //      the user pauses inside the iframe's JS and tries to access something inside the iframe via 
    //      __RECORD_REPLAY__?
    window.__RECORD_REPLAY__ = window.top.__RECORD_REPLAY__;
    window.__RECORD_REPLAY_ARGUMENTS__ = window.top.__RECORD_REPLAY_ARGUMENTS__;
  }
  catch (err) {
    // TODO: RUN-1990
    // window.top is not always accessible due to cross-origin restrictions.
    __RECORD_REPLAY_ARGUMENTS__.log(`[RuntimeError] gOnNewWindowScript failed: ${err.stack}`);
  }
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
    recordreplay::AutoDisallowEvents disallow("InitializeReplayScripts");

    // Run `gReplayScript`.
    RunScript(isolate, context, gReplayScript, InternalScriptURL);
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
