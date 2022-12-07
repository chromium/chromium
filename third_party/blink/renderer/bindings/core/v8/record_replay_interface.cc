// Copyright 2021 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/record_replay_interface.h"

#include "v8/third_party/inspector_protocol/crdtp/json.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/record_replay.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/resolve_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
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
extern void RecordReplayConfirmObjectHasId(v8::Isolate* isolate, v8::Local<v8::Context> cx,
                                           v8::Local<v8::Value> object);

} // namespace internal

} // namespace v8

namespace blink {

// using RemoteObjectIdTypeRaw = v8_inspector::String16;

// The actual type for RemoteObjectId
using RemoteObjectIdTypeRaw = std::u16string;

// The more convenient type that we use
using RemoteObjectIdType = WTF::String;

// Script which defines handlers for recorder commands, and is only loaded while
// replaying.
const char* gReplayScript = R""""(
(() => {

const Verbose = 1;
const VerboseCommands = Verbose;

const {
  log,
  setCDPMessageCallback,
  sendCDPMessage,
  setCommandCallback,
  setClearPauseDataCallback,
  addNewScriptHandler,
  getCurrentError,
  fromJsMakeDebuggeeValue,
  fromJsGetObjectByCdpId,

  // network
  getCurrentNetworkRequestEvent,
  getCurrentNetworkStreamData,

  // Blink, DOM and more
  // ?
} = __RECORD_REPLAY_ARGUMENTS__;

const gSourceMapData = new Map();

try {



// Save these before page code potentially overwrites them.
const JSON_stringify = JSON.stringify;
const JSON_parse = JSON.parse;

///////////////////////////////////////////////////////////////////////////////
// utils.js
///////////////////////////////////////////////////////////////////////////////

// Some of these are duplicated in gSourceMapScript, so watch out when making
// modifications to update both versions...

function assert(v, msg = "") {
  if (!v) {
    log(`[RuntimeError] Assertion failed ${msg} ${Error().stack}`);
    throw new Error("Assertion failed!");
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

///////////////////////////////////////////////////////////////////////////////
// message.js
///////////////////////////////////////////////////////////////////////////////

function initMessages() {
  setCDPMessageCallback(messageCallback);
  setCommandCallback(commandCallback);
  setClearPauseDataCallback(clearPauseDataCallback);
}

let gNextMessageId = 1;

let gCurrentMessageId;
let gCurrentMessageResult;

function sendMessage(method, params) {
  const id = gNextMessageId++;
  gCurrentMessageId = id;
  sendCDPMessage(JSON_stringify({ method, params, id }));
  gCurrentMessageId = undefined;
  if (gCurrentMessageResult?.result) {
    return gCurrentMessageResult.result;
  }
  if (gCurrentMessageResult?.error) {
    throw new Error(`${gCurrentMessageResult.error.message} (${gCurrentMessageResult.error.code})`);
  }
  return undefined;
}

const gEventListeners = new Map();

function addEventListener(method, callback) {
  gEventListeners.set(method, callback);
}

// TODO: rename all these CDP-related symbols to also have CDP in the name
function messageCallback(message) {
  try {
    message = JSON_parse(message);

    if (message.id) {
      assert(message.id == gCurrentMessageId);
      gCurrentMessageResult = message;
    } else {
      const listener = gEventListeners.get(message.method);
      if (listener) {
        listener(message.params);
      }
    }
  } catch (e) {
    log(`Message callback exception: ${e}`);
  }
}

///////////////////////////////////////////////////////////////////////////////
// main.js
///////////////////////////////////////////////////////////////////////////////

// Methods for interacting with the record/replay driver.

initMessages();
addEventListener("Runtime.consoleAPICalled", onConsoleAPICall);
sendMessage("Runtime.enable");

const CommandCallbacks = {
  "Graphics.getDevicePixelRatio": Graphics_getDevicePixelRatio,
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
  "DOM.getBoxModel": DOM_getBoxModel
};


function commandCallback(method, params) {
  if (!CommandCallbacks[method]) {
    log(`[RuntimeError][Command ${method}] Missing command callback: ${method}`);
    return {};
  }

  try {
    VerboseCommands && log(`[Command ${method}] Handling command, params=${JSON.stringify(params)}...`);
    const result = CommandCallbacks[method](params);
    VerboseCommands && log(`[Command ${method}] Handled command, result=${JSON.stringify(result)}`);
    return result;
  } catch (e) {
    log(`[RuntimeError][Command ${method}] ${e?.stack || e}`);
    return {};
  }
}

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

  // Get the protocol representation of the message arguments.
  const argumentValues = [];
  for (const arg of gLastConsoleAPICall.args || []) {
    argumentValues.push(cdpToRrpObject(arg));
  }

  let level = "info";
  switch (gLastConsoleAPICall.level) {
    case "warning":
      level = "warning";
      break;
    case "error":
      level = "error";
      break;
  }

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
    const obj = JSON.parse(getCurrentNetworkRequestEvent());
    return { data: obj };
  } catch (e) {
    log(`Error: getCurrentNetworkRequestEvent exception: ${e}`);
  }
}

function Target_getCurrentNetworkStreamData(params) {
  const data = getCurrentNetworkStreamData(params);
  if (data) {
    return { data };
  } else {
    log(`Error: getCurrentNetworkStreamData returned no data.`);
  }
}

function Target_topFrameLocation() {
  const { location } = sendMessage("Debugger.getTopFrameLocation");
  if (!location) {
    return {};
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
  // NOTE: this is a custom command we added
  const { callFrames } = sendMessage("Debugger.getCallFrames");
  return callFrames;
}


// Build a protocol Result object from a result/exceptionDetails CDP rval.
function buildRrpObjectResult({ result, exceptionDetails }) {
  const value = cdpToRrpObject(result);
  const protocolResult = { data: {} };

  if (exceptionDetails) {
    protocolResult.exception = value;
  } else {
    protocolResult.returned = value;
  }
  return { result: protocolResult };
}


function Pause_evaluateInFrame({ frameId, expression }) {
  const frames = getStackFrames();
  const index = +frameId;
  assert(index < frames.length);
  const frame = frames[index];

  const rv = doEvaluation();
  return buildRrpObjectResult(rv);

  function doEvaluation() {
    // In order to do the evaluation in the right frame, the same number of
    // frames need to be on V8's stack when we do the evaluation as when we got
    // the stack frames in the first place. The debugger agent extracts a frame
    // index from the ID it is given and uses that to walk the stack to the
    // frame where it will do the evaluation (see DebugStackTraceIterator).
    return sendMessage(
      "Debugger.evaluateOnCallFrame",
      {
        callFrameId: frame.callFrameId,
        expression,
      }
    );
  }
}

function Pause_evaluateInGlobal({ expression }) {
  const rv = sendMessage("Runtime.evaluate", { expression });
  return buildRrpObjectResult(rv);
}

// function onMaybeNewPause() {
// }

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
  const rv = sendMessage("Debugger.getPendingException", {});
  return { exception: cdpToRrpObject(rv.exception), data: {} };
}

function Pause_getObjectPreview({ object, level = "full" }) {
  const objectData = createPauseObject(object, level);
  return { data: { objects: [objectData] } };
}

function Pause_getObjectProperty({ object, name }) {
  const cdpObj = getCdpObjectByRrpId(object);
  const rv = sendMessage(
    "Runtime.callFunctionOn",
    {
      functionDeclaration: `function() { return this["${name}"] }`,
      objectId: cdpObj.objectId,
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
// object.js
// Manage association between remote objects and protocol object IDs.
///////////////////////////////////////////////////////////////////////////////


// Map protocol ObjectId => RemoteObject
/**
 * @type {Map<CDP.Runtime.RemoteObject, string>}
 */
const gCdpObjectsByRrpId = new Map();

/**
 * @type {Map<string, string>}
 */
const gRrpIdByCdpId = new Map();
/**
 * @type {Map<Object, string>}
 */
const gObjectsByRrpId = new Map();
/**
 * @type {Map<Object, string>}
 */
const gRrpIdByPlainObject = new Map();
/**
 * @type {Map<string, Object>}
 */
const gPlainObjectByRrpId = new Map();

let gLastRrpId = 0;

// Map protocol ObjectId => Debugger.Scope
const gCdpScopesByRrpId = new Map();

function clearPauseDataCallback() {
  try {
    gCdpObjectsByRrpId.clear();
    gRrpIdByCdpId.clear();
    gObjectsByRrpId.clear();
    gRrpIdByPlainObject.clear();
    gPlainObjectByRrpId.clear();
    gCdpScopesByRrpId.clear();
    gLastBoundingClientRectsByObjectId.clear();
    gLastRrpId = 0;
  } catch (e) {
    log(`Error: clearPauseDataCallback exception: ${e}`);
  }
}

/**
 * Creates and returns a new `RemoteObject` for given JS object.
 * 
 * @return {CDP.Runtime.RemoteObject}
 * @see https://chromedevtools.github.io/devtools-protocol/tot/Runtime/#type-RemoteObject
 */
function makeDebuggeeValue(plainObject) {
  assert(!plainObject.objectId);
  const remoteObjectStr = fromJsMakeDebuggeeValue(plainObject);
  const remoteObject = JSON.parse(remoteObjectStr);
  assert(remoteObject.objectId);
  return remoteObject;
}

/**
 * @param {Object} plainObject
 * @return {number}
 */
function registerPlainObject(plainObject) {
  let rrpId = gRrpIdByPlainObject.get(plainObject);
  if (!rrpId) {
    // → ask V8InspectorSession to wrap plainObject (gets CDP.RemoteObject)
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
  let plainObject = gRrpIdByPlainObject.get(rrpId);
  if (!plainObject) {
    // (if this was a ref type, registration would already have been handled in `registerCdpObject` ↓)
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

  // new RrpId
  const existingRrpId = rrpId;
  rrpId ||= ++gLastRrpId + '';  // coerce to string
  gCdpObjectsByRrpId.set(rrpId, cdpObject);
  gRrpIdByCdpId.set(cdpId, rrpId);
  if (plainObject && !existingRrpId) {
    gRrpIdByPlainObject.set(plainObject, rrpId);
    gPlainObjectByRrpId.set(rrpId, plainObject);
  }

  const { className, description, type } = gCdpObjectsByRrpId.get(rrpId) || {};
  return rrpId;
}

/**
 * 
 * @return {CDP.Runtime.RemoteObject}
 */
function getCdpObjectByRrpId(rrpId) {
  const cdpObject = gCdpObjectsByRrpId.get(rrpId);
  assert(cdpObject);
  return cdpObject;
}

// Strings longer than this will be truncated when creating protocol values.
const MaxStringLength = 10000;

const cdpRefTypes = ['object', 'function'];
function isCdpRefType(cdpObject) {
  return cdpRefTypes.includes(cdpObject.type);
}

function cdpToRrpObject(cdpObject) {
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

///////////////////////////////////////////////////////////////////////////////
// preview.js
///////////////////////////////////////////////////////////////////////////////

// Logic for creating object previews for the record/replay protocol.

/**
 * @return {RRP.Pause.Object}
 * @see https://static.replay.io/protocol/tot/Pause/#type-Object
 */
function createPauseObject(rrpId, level) {
  const cdpObj = getCdpObjectByRrpId(rrpId);
  // NOTE: `subtype` is not reliably available, due to a divergence check in V8 → `value-mirror.cc`
  const className = cdpObj.subtype == "proxy" ? "Proxy" : (cdpObj.className || "Function");

  // NOTE: `persistentId` is added in V8 → `injected-script.cc`
  const { persistentId } = cdpObj;

  let preview;
  if (level != "none") {
    preview = new ProtocolObjectPreview(cdpObj, level).fill();
  }

  return { objectId: rrpId, persistentId, className, preview };
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

function ProtocolObjectPreview(obj, level) {
  this.cdpObj = obj;
  this.level = level;
  this.overflow = false;
  this.numItems = 0;
  this.extra = {};
}

ProtocolObjectPreview.prototype = {
  canAddItem(force) {
    if (!force && this.numItems >= MaxItems[this.level]) {
      this.overflow = true;
      return false;
    }
    this.numItems++;
    return true;
  },

  addProperty(property, force) {
    if (!this.canAddItem(force)) {
      return;
    }
    if (!this.properties) {
      this.properties = [];
    }
    this.properties.push(property);
  },

  addContainerEntry(entry) {
    if (!this.canAddItem()) {
      return;
    }
    if (!this.containerEntries) {
      this.containerEntries = [];
    }
    this.containerEntries.push(entry);
  },

  fill() {
    // NOTE: we could also use "Runtime.evaluate" with `{ generatePreview: true }`
    const allProperties = sendMessage("Runtime.getProperties", {
      objectId: this.cdpObj.objectId,
      ownProperties: true,
      generatePreview: false,
    });
    const properties = allProperties.result;

    // Add data for DOM/CSS objects
    this.extra = previewBlinkObject(this.cdpObj, allProperties) || {};

    // Add class-specific data.
    const previewer = CustomPreviewers[this.cdpObj.className];
    const requiredProperties = [];
    if (previewer) {
      for (const entry of previewer) {
        if (typeof entry == "string") {
          requiredProperties.push(entry);
        } else {
          entry.call(this, allProperties);
        }
      }
    }

    let prototype;
    for (const prop of properties) {
      if (prop.name == "__proto__") {
        prototype = prop;
      } else {
        const protocolProperty = createProtocolPropertyDescriptor(prop);
        const force = requiredProperties.includes(prop.name);
        this.addProperty(protocolProperty, force);
      }
    }

    let prototypeRrpId;
    if (prototype && prototype.value && prototype.value.objectId) {
      prototypeRrpId = registerCdpObject(prototype.value);
      // TODO: in gecko-dev, we also `addPrototypeGetterValues` - should we do this here, too?
    }
    return {
      prototypeId: prototypeRrpId,
      overflow: (this.overflow && this.level != "full") ? true : undefined,
      properties: this.properties,
      containerEntries: this.containerEntries,
      ...this.extra,
    };
  },
};

// Get a count from an object description like "Array(42)"
function getDescriptionCount(description) {
  const match = /\((\d+)\)/.exec(description || "");
  if (match) {
    return +match[1];
  }
}

function previewBlinkObject(cdpObject, allProperties) {
  const cdpId = cdpObject.objectId;
  const rrpId = gRrpIdByCdpId.get(cdpId);
  assert(rrpId);
  const plainObject = getPlainObjectByRrpId(rrpId);

  /**
   * @see https://github.com/replayio/gecko-dev/blob/592992ff7e15cb8ad1dd6fb109f19bd3523cd452/devtools/server/actors/replay/module.js#L1937
   */
  if (plainObject instanceof Node) {
    return {
      node: previewBlinkNode(plainObject)
    }
  }

  // TODO: preview other blink objects
  // Issue: https://linear.app/replay/issue/RUN-861/add-dom-related-props-to-pausegetobjectpreview-and-fix-objectid
  
}

function previewBlinkNode(node) {
  let attributes, pseudoType;
  if (node instanceof Element) {
    attributes = [];
    for (const { name, value } of node.attributes) {
      attributes.push({ name, value });
    }
    // TODO: We cannot access pseudo elements using the JS DOM API.
    // Issue: https://linear.app/replay/issue/RUN-953/dom-feature-add-pseudo-elements
    // pseudoType = node.localName;
  }

  let style;
  if (node.style) {
    style = registerPlainObject(node.style);
  }

  let parentNode;
  if (node.parentNode) {
    parentNode = registerPlainObject(node.parentNode);
  } else if (node.defaultView && node.defaultView.parent != node.defaultView) {
    /**
     * Nested documents use the parent element instead of null.
     * 
     * TODO: will need more work here to support multi-CSP iframes
     * Issue: https://linear.app/replay/issue/RUN-954/dom-feature-support-multi-cspcross-origin-iframes
     */
    const iframes = node.defaultView.parent.document.getElementsByTagName(
      "iframe"
    );
    const iframe = [...iframes].find((f) => f.contentDocument == node);
    if (iframe) {
      parentNode = registerPlainObject(iframe);
    }
  }

  let childNodes;
  if (node.nodeName == "IFRAME") {
    // Treat an iframe's content document as one of its child nodes.
    childNodes = [registerPlainObject(node.contentDocument)];
  } else if (node.childNodes.length) {
    childNodes = [...node.childNodes].map((n) => registerPlainObject(n));
  }

  let documentURL;
  if (node.nodeType == Node.DOCUMENT_NODE) {
    documentURL = node.URL;
  }

  return {
    nodeType: node.nodeType,
    nodeName: node.nodeName,
    nodeValue: typeof node.nodeValue === "string" ? node.nodeValue : undefined,
    isConnected: node.isConnected,
    attributes,
    pseudoType,
    style,
    parentNode,
    childNodes,
    documentURL,
  };
}

function previewTypedArray() {
  // The typed array size isn't available from the object's own property
  // information, except by parsing the object description.
  const length = getDescriptionCount(this.obj.description);
  if (length !== undefined) {
    this.addProperty({ name: "length", value: length }, /* force */ true);
  }
}

function previewSetMap(allProperties) {
  if (!allProperties.internalProperties) {
    return;
  }

  const internal = allProperties.internalProperties.find(prop => prop.name == "[[Entries]]");
  if (!internal || !internal.value || !internal.value.objectId) {
    return;
  }

  // Get the container size from the length of the entries.
  const size = getDescriptionCount(internal.value.description);
  if (size !== undefined) {
    this.extra.containerEntryCount = size;
    if (["Set", "Map"].includes(this.obj.className)) {
      this.addProperty({ name: "size", value: size }, /* force */ true);
    }
  }

  const entries = sendMessage("Runtime.getProperties", {
    objectId: internal.value.objectId,
    ownProperties: true,
    generatePreview: false,
  }).result;

  for (const entry of entries) {
    if (entry.value.subtype == "internal#entry") {
      const entryProperties = sendMessage("Runtime.getProperties", {
        objectId: entry.value.objectId,
        ownProperties: true,
        generatePreview: false,
      }).result;
      const key = entryProperties.find(eprop => eprop.name == "key");
      const value = entryProperties.find(eprop => eprop.name == "value");
      if (value) {
        this.addContainerEntry({
          key: key ? cdpToRrpObject(key.value) : undefined,
          value: cdpToRrpObject(value.value),
        });
      }
    }
    if (this.overflow) {
      break;
    }
  }
}

function previewRegExp() {
  this.extra.regexpString = this.obj.description;
}

function previewDate() {
  const dateTime = Date.parse(this.obj.description);
  if (!Number.isNaN(dateTime)) {
    this.extra.dateTime = dateTime;
  }
}

function previewError() {
  this.addProperty({ name: "name", value: this.obj.className }, /* force */ true);
}

const ErrorProperties = [
  "message",
  "stack",
  previewError,
];

function previewFunction(allProperties) {
  const nameProperty = allProperties.result.find(prop => prop.name == "name");
  if (nameProperty) {
    this.extra.functionName = nameProperty.value.value;
  }

  const locationProperty = allProperties.internalProperties.find(
    prop => prop.name == "[[FunctionLocation]]"
  );
  if (locationProperty) {
    this.extra.functionLocation = createProtocolLocation(locationProperty.value.value);
  }
}



const CustomPreviewers = {
  Array: ["length"],
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
};

function createProtocolPropertyDescriptor(desc) {
  const { name, value, writable, get, set, configurable, enumerable, symbol } = desc;

  const rv = value ? cdpToRrpObject(value) : {};
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

  if (symbol) {
    rv.isSymbol = true;
  }

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
    this: cdpToRrpObject(cdpFrame.this),
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

    const properties = sendMessage("Runtime.getProperties", {
      objectId: cdpScope.object.objectId,
      ownProperties: true,
      generatePreview: false,
    }).result;
    for (const { name, value: cdpProp } of properties) {
      const rrpProp = cdpToRrpObject(cdpProp);
      bindings.push({ ...rrpProp, name });
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
  function StackingContextElement(node, parent, offset, style, clipBounds) {
    assert(node.nodeType == Node.ELEMENT_NODE);

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
  function StackingContext(window, root, offset, realStackingContext) {
    this.window = window;
    this.id = gNextStackingContextId++;

    this.realStackingContext = realStackingContext || this;

    // Offset relative to the outer window of the window containing this context.
    this.offset = offset || { left: 0, top: 0 };

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
      const elem = new StackingContextElement(node, parentElem, offset, style, clipBounds);
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
      if (elem.raw.tagName == "IFRAME") {
        const { left, top } = elem.raw.getBoundingClientRect();
        this.addContext(elem, undefined, left, top);
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
          this.addContext(elem);
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
            this.addContext(elem, this.realStackingContext);
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
        this.addContext(elem, this.realStackingContext);
        this.addFloatingElement(elem);
        return;
      }

      const display = elem.style.getPropertyValue("display");
      if (display == "inline-block" || display == "inline-table") {
        // Group the element and its descendants.
        this.addContext(elem, this.realStackingContext);
        this.addNonPositionedElement(elem);
        return;
      }

      this.addNonPositionedElement(elem);
      this.addChildrenWithParent(elem);
    },

    addContext(elem, realStackingContext, left = 0, top = 0) {
      if (elem.context) {
        assert(!left && !top);
        return;
      }
      const offset = {
        left: this.offset.left + left,
        top: this.offset.top + top,
      };
      elem.context = new StackingContext(this.window, elem, offset, realStackingContext);
    },

    addZIndexElement(elem, index) {
      const existing = this.zIndexElements.get(index);
      if (existing) {
        existing.push(elem);
      } else {
        this.zIndexElements.set(index, [elem]);
      }
    },

    addPositionedElement(elem) {
      this.positionedElements.push(elem);
    },

    addFloatingElement(elem) {
      this.floatingElements.push(elem);
    },

    addNonPositionedElement(elem) {
      this.nonPositionedElements.push(elem);
    },

    addChildren(parentNode) {
      for (const child of parentNode.children) {
        this.add(child, undefined, this.offset);
      }
    },

    addChildrenWithParent(parentElem) {
      for (const child of parentElem.raw.children) {
        this.add(child, parentElem, this.offset);
      }
    },

    // Get the elements in this context ordered back-to-front.
    flatten() {
      const rv = [];

      const pushElements = (elems) => {
        for (const elem of elems) {
          if (elem.context && elem.context != this) {
            rv.push(...elem.context.flatten());
          } else {
            rv.push(elem);
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
  function shiftRect(rect, offset) {
    return {
      left: rect.left !== undefined ? offset.left + rect.left : undefined,
      top: rect.top !== undefined ? offset.top + rect.top : undefined,
      right: rect.right !== undefined ? offset.left + rect.right : undefined,
      bottom: rect.bottom !== undefined ? offset.top + rect.bottom : undefined,
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

  const gLastBoundingClientRectsByObjectId = new Map();

  function getLastBoundingClientRect(objectId) {
    return gLastBoundingClientRectsByObjectId.get(objectId);
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

        const { left, top, right, bottom } = shiftRect(elem.raw.getBoundingClientRect(), elem.offset);

        if (left >= right || top >= bottom) {
          return null;
        }

        const clipBounds = shiftRect(elem.clipBounds, elem.offset);
        // ignore elements that are completely outside their clipBounds
        if (
          clipBounds.left > right ||
          clipBounds.top > bottom ||
          clipBounds.right < left ||
          clipBounds.bottom < top
        ) {
          return null;
        }
        // only return the clipBounds that actually affect this element
        if (clipBounds.left === undefined || clipBounds.left <= left) {
          delete clipBounds.left;
        }
        if (clipBounds.top === undefined || clipBounds.top <= top) {
          delete clipBounds.top;
        }
        if (clipBounds.right === undefined || clipBounds.right >= right) {
          delete clipBounds.right;
        }
        if (clipBounds.bottom === undefined || clipBounds.bottom >= bottom) {
          delete clipBounds.bottom;
        }

        const rects = [...elem.raw.getClientRects()]
          .map(rect => shiftRect(rect, elem.offset))
          .map(({ left, top, right, bottom }) => {
            if (left >= right || top >= bottom) {
              return null;
            }
            return [left, top, right, bottom];
          })
          .filter((v) => !!v);

        const v = {
          node: id,
          rect: [left, top, right, bottom],
        };
        if (rects.length > 1) {
          v.rects = rects;
        }
        if (Object.keys(clipBounds).length > 0) {
          v.clipBounds = clipBounds;
        }
        if (elem.style?.getPropertyValue("visibility") === "hidden") {
          v.visibility = "hidden";
        }
        if (elem.style?.getPropertyValue("pointer-events") === "none") {
          v.pointerEvents = "none";
        }

        gLastBoundingClientRectsByObjectId.set(id, v);

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
    if (!gLastBoundingClientRectsByObjectId.size) {
      // compute all basic bounding client rect sizes
      DOM_getAllBoundingClientRects();
    }
    const rectInfo = getLastBoundingClientRect(node)
    const rect = rectInfo?.rect || [0, 0, 0, 0];

    return { rect };
  }

  /** ###########################################################################
   * {@link DOM_getBoxModel}
   * ##########################################################################*/

  /**
   * @see https://static.replay.io/protocol/tot/DOM/#type-BoxModel
   */
  function DOM_getBoxModel({ node }) {

    // TODO: consider using domAgent->getContentQuads for this instead. Should allow for better accuracy
    //   Issue: https://linear.app/replay/issue/RUN-886/nodepicker-improve-highlight-on-hover

    if (!gLastBoundingClientRectsByObjectId.size) {
      // compute all basic bounding client rect sizes
      DOM_getAllBoundingClientRects();
    }
    const rectInfo = getLastBoundingClientRect(node)
    const rects = rectInfo?.rects || 
      (rectInfo?.rect ? [rectInfo.rect] : [[0, 0, 20, 20]]);

    // hackfix: simply use the normal (not tight) bounding rects for all for now
    const model = { node };
    for (const box of ["content", "padding", "border", "margin"]) {
      const quads = [];
      for (const rect of rects) {
        const [left, top, right, bottom] = rect;
        quads.push(left, top, right, top, right, bottom, left, bottom);
      }
      model[box] = quads;
    }


    // const model = { node };
    // for (const box of ["content", "padding", "border", "margin"]) {
    //   const compactQuads = [];
    //   // https://hacks.mozilla.org/2014/03/introducing-the-getboxquads-api/
    //   if (nodeObj.getBoxQuads) {
    //     const quads = nodeObj.getBoxQuads({
    //       box,
    //       relativeTo: window.document,
    //     });
    //     for (const { p1, p2, p3, p4 } of quads) {
    //       compactQuads.push(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, p4.x, p4.y);
    //     }
    //   // }
    //   model[box] = compactQuads;
    // }

    return { model };
  }

} catch (e) {
  log(`Error: Initialization exception ${e}`);
}

})();

)"""";

/** ###########################################################################
 * gSourceMapScript
 * ##########################################################################*/

// Script which sets a handler for collecting source maps from scripts in the
// recording. Runs when recording/replaying if source map collection is enabled.
const char* gSourceMapScript = R""""(

(() => {

const {
  log,
  getRecordingId,
  sha256DigestHex,
  writeToRecordingDirectory,
  addRecordingEvent,
  addNewScriptHandler,
  getScriptSource
} = __RECORD_REPLAY_ARGUMENTS__;

addNewScriptHandler(async (scriptId, sourceURL, relativeSourceMapURL) => {
  if (!relativeSourceMapURL || relativeSourceMapURL.startsWith("data:"))
    return;

  const urls = getSourceMapURLs(sourceURL, relativeSourceMapURL);
  if (!urls)
    return;

  const { sourceMapURL, sourceMapBaseURL } = urls;

  let sourceMap;
  try {
    sourceMap = await fetchText(sourceMapURL);
  } catch (err) {
    log(`Failed to read sourcemap ${sourceMapURL}: ${err.message}`);
  }
  if (!sourceMap) {
    return;
  }

  const scriptSource = getScriptSource(scriptId);

  const recordingId = getRecordingId();
  if (!recordingId) {
    // The recording has been invalidated.
    return;
  }

  const id = String(Math.floor(Math.random() * 10000000000));
  const name = `sourcemap-${id}.map`;
  const path = await writeToRecordingDirectory(name, sourceMap);
  await addRecordingEvent(JSON.stringify({
    kind: "sourcemapAdded",
    path,
    recordingId,
    id,
    url: sourceMapURL,
    baseURL: sourceMapBaseURL,
    targetContentHash: typeof scriptSource === "string"
      ? makeAPIHash(scriptSource)
      : undefined,
    targetURLHash: sourceURL ? makeAPIHash(sourceURL) : undefined,
    targetMapURLHash: makeAPIHash(sourceMapURL),
  }));

  const { sources } =
    collectUnresolvedSourceMapResources(sourceMap, sourceMapURL, sourceURL);

  for (const { offset, url } of sources) {
    let sourceContent;
    try {
      sourceContent = await fetchText(url);
    } catch (err) {
      log(`Failed to read original source ${url}: ${err.message}`);
      continue;
    }
    const sourceId = String(Math.floor(Math.random() * 10000000000));
    const name = `original-source-${id}-${sourceId}`;
    const path = await writeToRecordingDirectory(name, sourceContent);
    await addRecordingEvent(JSON.stringify({
      kind: "originalSourceAdded",
      path,
      recordingId,
      parentId: id,
      parentOffset: offset,
    }));
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

  function logError(msg) {
    log(`${msg} (${mapURL}:${sourceOffset})`);
  }

  const unresolvedSources = [];
  let sourceOffset = 0;

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

  return {
    sources: unresolvedSources,
  };
}

///////////////////////////////////////////////////////////////////////////////
// utils.js
///////////////////////////////////////////////////////////////////////////////

// Some of these are duplicated in gReplayScript, so watch out when making
// modifications to update both versions...

function assert(v) {
  if (!v) {
    log(`Error: Assertion failed ${Error().stack}`);
    throw new Error("Assertion failed");
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

// Script which sets a handler for collecting source maps from scripts in the
// recording. Runs when recording/replaying if source map collection is enabled.
const char* gReactDevtoolsScript = R""""(

(() => {

const hook = {
  supportsFiber: true,
  inject,
  onCommitFiberUnmount,
  onCommitFiberRoot,
  onPostCommitFiberRoot,
};

Object.defineProperty(window, "__REACT_DEVTOOLS_GLOBAL_HOOK__", {
  configurable: false,
  enumerable: false,
  get() {
    return hook;
  }
});

let uidCounter = 0;

function inject(renderer) {
  const id = ++uidCounter;
  window.__RECORD_REPLAY_ANNOTATION_HOOK__("react-devtools-hook", "inject");
  return id;
}

function onCommitFiberUnmount(rendererID, fiber) {
  window.__RECORD_REPLAY_ANNOTATION_HOOK__("react-devtools-hook", "commit-fiber-unmount");
}

function onCommitFiberRoot(rendererID, root, priorityLevel) {
  window.__RECORD_REPLAY_ANNOTATION_HOOK__("react-devtools-hook", "commit-fiber-root");
}

function onPostCommitFiberRoot(rendererID, root) {
  window.__RECORD_REPLAY_ANNOTATION_HOOK__("react-devtools-hook", "post-commit-fiber-root");
}

})();

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

// Function to invoke on CDP responses and events.
static v8::Eternal<v8::Function>* gCDPMessageCallback;

static void SetCDPMessageCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(!gCDPMessageCallback);
  v8::Isolate* isolate = args.GetIsolate();
  CHECK(args[0]->IsFunction());
  v8::Local<v8::Function> callback = args[0].As<v8::Function>();
  gCDPMessageCallback = new v8::Eternal<v8::Function>(isolate, callback);
}

static void SendMessageToFrontend(const v8_inspector::StringView& message) {
  CHECK(v8::IsMainThread());

  CHECK(gCDPMessageCallback);
  CHECK(!message.is8Bit());

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (!isolate->InContext() || ScriptForbiddenScope::IsScriptForbidden()) {
    // We're never interested in messages sent at these times.
    return;
  }

  v8::HandleScope scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> arg = v8::String::NewFromTwoByte(isolate, message.characters16(),
                                                        v8::NewStringType::kNormal,
                                                        message.length()).ToLocalChecked();
  v8::Local<v8::Function> callback = gCDPMessageCallback->Get(isolate);
  v8::MaybeLocal<v8::Value> rv = callback->Call(context, v8::Undefined(isolate), 1, &arg);
  CHECK(!rv.IsEmpty());
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

static v8_inspector::V8Inspector* gInspector;
static v8_inspector::V8InspectorSession* gInspectorSession;

void RecordReplayRegisterV8Inspector(v8_inspector::V8Inspector* inspector,
                                     v8::Isolate* isolate) {
  if (v8::IsMainThread()) {
    gInspector = inspector;

    // For now we only connect to the first frame.
    static int ContextGroupId = 1;

    gInspectorSession = gInspector->connect(ContextGroupId,
                                            new InspectorChannel(),
                                            v8_inspector::StringView()).release();
  }
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
  CHECK(v8::IsMainThread());
  CHECK(gInspectorSession);
  CHECK(args.Length() == 1 && args[0]->IsString() &&
        "must be called with a single string");
  v8::String::Utf8Value message(args.GetIsolate(), args[0]);

  std::string nmessage(*message);
  v8_inspector::StringView messageView((const uint8_t*)nmessage.c_str(), nmessage.length());
  gInspectorSession->dispatchProtocolMessage(messageView);
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

static void WriteToRecordingDirectory(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 2 && args[0]->IsString() && args[1]->IsString() &&
        "must be called with two strings");
  v8::Isolate* isolate = args.GetIsolate();
  v8::String::Utf8Value filename(isolate, args[0]);
  v8::String::Utf8Value content(isolate, args[1]);

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

static void GetPersistentId(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() >= 1 && recordreplay::HasDivergedFromRecording()) {
    int id = v8::internal::RecordReplayObjectId(args.GetIsolate(),
                                                args.GetIsolate()->GetCurrentContext(),
                                                args[0], /* allow_create */ false);
    if (id) {
      args.GetReturnValue().Set(v8::Number::New(args.GetIsolate(), id));
    }
  }
}

static void CheckPersistentId(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() >= 1) {
    v8::internal::RecordReplayConfirmObjectHasId(args.GetIsolate(),
                                                 args.GetIsolate()->GetCurrentContext(),
                                                 args[0]);
  }
}

static void GetCurrentError(const v8::FunctionCallbackInfo<v8::Value>& args);

extern "C" void V8RecordReplayFinishRecording();



// When JS assertions are enabled, this callback is used to get any pointer ID
// associated with a given API object.
static int GetAPIObjectIdCallback(v8::Local<v8::Object> object) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  const WrapperTypeInfo* infos[] = {
    // ScriptWrappable itself doesn't have wrapper type info, so check subclasses.
    Node::GetStaticWrapperTypeInfo(),
    Event::GetStaticWrapperTypeInfo(),
    CSSStyleDeclaration::GetStaticWrapperTypeInfo()
  };
  for (const WrapperTypeInfo* info : infos) {
    if (V8PerIsolateData::From(isolate)->HasInstance(info, object)) {
      ScriptWrappable* wrappable = ToScriptWrappable(object);
      return wrappable->RecordReplayId();
    }
  }
  return 0;
}

static void SetDataProperty(v8::Isolate* isolate,
                            v8::Local<v8::Object> obj,
                            const char* property,
                            v8::Local<v8::Value> value) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  obj->Set(context, ToV8String(isolate, property), value).Check();
}


/** ###########################################################################
 * Networking
 * ##########################################################################*/

// Represents a known network request.  Created and added to
// `gActiveNetworkRequests` when the request is first seen.  Removed
// when the request finishes or fails.
struct NetworkRequestStatus {
  bool response_received;
  size_t response_data_received;
  NetworkRequestStatus()
  : response_received(false),
    response_data_received(0)
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

static void HandleNetworkPrepareRequestEvent(const base::DictionaryValue& info) {
  CHECK(gActiveNetworkRequests);
  std::string request_id = *info.FindPath("requestId")->GetIfString();
  gActiveNetworkRequests->insert({ request_id, NetworkRequestStatus() });

  base::DictionaryValue event;
  event.SetString("kind", "request");
  event.Set("requestUrl", std::unique_ptr<base::Value>(info.FindPath("requestUrl")->DeepCopy()));
  event.Set("requestMethod", std::unique_ptr<base::Value>(info.FindPath("requestMethod")->DeepCopy()));
  event.Set("requestHeaders", std::unique_ptr<base::Value>(info.FindPath("requestHeaders")->DeepCopy()));
  const base::Value* request_cause_value = info.FindPath("requestCause");
  if (request_cause_value) {
    event.Set("requestCause", std::unique_ptr<base::Value>(request_cause_value->DeepCopy()));
  }

  gCurrentNetworkRequestEvent = &event;
  recordreplay::OnNetworkRequestEvent(request_id.c_str());
  gCurrentNetworkRequestEvent = nullptr;
}

static std::string MakeRequestIdentifier(uint64_t identifier) {
  char request_id[64];
  snprintf(request_id, 64, "%d.%lu", (int) getpid(), identifier);
  return std::string(request_id);
}

static void HandleNetworkDidReceiveResponseEvent(const base::DictionaryValue& info) {
  CHECK(gActiveNetworkRequests);
  uint64_t identifier =
    *info.FindPath("identifier")->GetIfDouble();
  std::string request_id = MakeRequestIdentifier(identifier);
  auto requestInfo = gActiveNetworkRequests->find(request_id);
  if (requestInfo == gActiveNetworkRequests->end()) {
    recordreplay::Print("Unknown request received response: %s",
      request_id.c_str());
    return;
  }

  gActiveNetworkRequests->insert({ request_id, NetworkRequestStatus() });
  base::DictionaryValue event;
  event.SetString("kind", "response");
  event.Set("responseHeaders", std::unique_ptr<base::Value>(
    info.FindPath("responseHeaders")->DeepCopy()
  ));
  event.Set("responseProtocolVersion", std::unique_ptr<base::Value>(
    info.FindPath("responseProtocolVersion")->DeepCopy()
  ));
  event.Set("responseStatus", std::unique_ptr<base::Value>(
    info.FindPath("responseStatus")->DeepCopy()
  ));
  event.Set("responseStatusText", std::unique_ptr<base::Value>(
    info.FindPath("responseStatusText")->DeepCopy()
  ));
  event.Set("responseFromCache", std::unique_ptr<base::Value>(
    info.FindPath("responseFromCache")->DeepCopy()
  ));

  gCurrentNetworkRequestEvent = &event;
  recordreplay::OnNetworkRequestEvent(request_id.c_str());
  gCurrentNetworkRequestEvent = nullptr;
}

static void HandleNetworkDidFinishLoadingEvent(const base::DictionaryValue& info) {
  CHECK(gActiveNetworkRequests);
  uint64_t identifier =
    *info.FindPath("identifier")->GetIfDouble();
  std::string request_id = MakeRequestIdentifier(identifier);
  auto request_info = gActiveNetworkRequests->find(request_id);
  if (request_info == gActiveNetworkRequests->end()) {
    recordreplay::Print("Unknown request finished loading: %s",
      request_id.c_str());
    return;
  }

  base::DictionaryValue event;
  event.SetString("kind", "request-done");
  event.Set("encodedBodySize", std::unique_ptr<base::Value>(
    info.FindPath("encodedBodySize")->DeepCopy()
  ));
  event.Set("decodedBodySize", std::unique_ptr<base::Value>(
    info.FindPath("decodedBodySize")->DeepCopy()
  ));

  gCurrentNetworkRequestEvent = &event;
  recordreplay::OnNetworkRequestEvent(request_id.c_str());
  gCurrentNetworkRequestEvent = nullptr;
}

static void HandleNetworkDidFailLoadingEvent(const base::DictionaryValue& info) {
  CHECK(gActiveNetworkRequests);
  uint64_t identifier =
    *info.FindPath("identifier")->GetIfDouble();
  std::string request_id = MakeRequestIdentifier(identifier);
  auto requestInfo = gActiveNetworkRequests->find(request_id);
  if (requestInfo == gActiveNetworkRequests->end()) {
    recordreplay::Print("Unknown request failed loading: %s",
      request_id.c_str());
    return;
  }

  base::DictionaryValue event;
  event.SetString("kind", "request-failed");
  event.Set("requestFailedReason", std::unique_ptr<base::Value>(
    info.FindPath("requestFailedReason")->DeepCopy()
  ));

  gCurrentNetworkRequestEvent = &event;
  recordreplay::OnNetworkRequestEvent(request_id.c_str());
  gCurrentNetworkRequestEvent = nullptr;
}

static void HandleNetworkDidReceiveDataEvent(const base::DictionaryValue& info) {
  CHECK(gActiveNetworkRequests);
  CHECK(gCurrentNetworkStreamData);
  // Get request info.
  uint64_t identifier =
    *info.FindPath("identifier")->GetIfDouble();
  std::string request_id = MakeRequestIdentifier(identifier);
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
 * DOM
 * @see https://static.replay.io/protocol/tot/DOM/
 * @see https://chromedevtools.github.io/devtools-protocol/tot/DOM/
 * ##########################################################################*/

static LocalFrame* gLocalFrame;

/**
 * NOTE: Since the `RemoteObject` type is not publicly exposed, we cannot easily access it in CPP space.
 * We thus only use it in JS.
 * This basically emulates gecko's `makeDebuggeeValue` (but requires an extra parse).
 */
static void fromJsMakeDebuggeeValue(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsObject() &&
        "must be called with a single object");
  v8::Isolate* isolate = args.GetIsolate();
  auto context = isolate->GetCurrentContext();
  auto obj = args[0]->ToObject(context).ToLocalChecked();

  const String object_group("console"); // NOTE: object_group is used for cleaning up
  auto generatePreview = false;

  // NOTE: This always creates (and deletes) a new `RemoteObject` and binds it
  // to a new id. RemoteObjectIdTypeRaw remoteObjectId =
  // v8_inspector::StringView result =
  // RemoteObjectIdTypeRaw result = gInspectorSession->wrapObjectGetObjectId(
  //     context, obj, ToV8InspectorStringView(object_group), false);
  // auto converted = String(result.c_str(), result.length());
  auto result = gInspectorSession->wrapObject(
      context, obj, ToV8InspectorStringView(object_group), generatePreview);

  // deserialize + send to JS
  std::vector<uint8_t> cbor;
  result->AppendSerialized(&cbor);

  if (cbor.size() > 1) {
    /**
     * This is based on other code that uses `wrapObject` and sends the result to JS.
     * @see
     * https://github.com/replayio/chromium-v8/blob/b38bf5b0b1f149f7af3fd90a2ce12344e7191d03/src/inspector/custom-preview.cc#L123
     */
    std::vector<uint8_t> json;
    v8_crdtp::json::ConvertCBORToJSON(v8_crdtp::SpanFrom(cbor), &json);
    auto remoteObjectStr =
        v8::String::NewFromOneByte(isolate, json.data(),
                                  v8::NewStringType::kNormal, json.size())
            .ToLocalChecked();
    args.GetReturnValue().Set(remoteObjectStr);
  }
  else {
    args.GetReturnValue().SetNull();
  }
}

static void fromJsGetObjectByCdpId(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() && "[RuntimeError] must be called with a single string");

  v8::Isolate* isolate = args.GetIsolate();
  auto context = isolate->GetCurrentContext();

  // convert v8::String → v8::String::Utf8Value → v8_inspector::StringView
  // TODO: can this be improved?
  v8::String::Utf8Value cdpId(isolate, args[0]);
  const uint8_t* cdpIdPtr = reinterpret_cast<const uint8_t*>(*cdpId);
  v8_inspector::StringView cdpIdV8(cdpIdPtr, cdpId.length());

  v8::Local<v8::Value> plainObject;
  std::unique_ptr<v8_inspector::StringBuffer> error;
  if (!gInspectorSession->unwrapObject(&error, cdpIdV8, &plainObject, &context,
                                       nullptr)) {
    recordreplay::Print("[RuntimError] could not lookup cdpId: %s",
                        ToCoreString(error->string()).Ascii().c_str());
    args.GetReturnValue().SetNull();
  } else {
    args.GetReturnValue().Set(plainObject);
  }
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
  } else if (!strcmp(name, "Network.DidReceiveResponse")) {
    HandleNetworkDidReceiveResponseEvent(base::Value::AsDictionaryValue(val));
  } else if (!strcmp(name, "Network.DidFinishLoading")) {
    HandleNetworkDidFinishLoadingEvent(base::Value::AsDictionaryValue(val));
  } else if (!strcmp(name, "Network.DidFailLoading")) {
    HandleNetworkDidFailLoadingEvent(base::Value::AsDictionaryValue(val));
  } else if (!strcmp(name, "Network.DidReceiveData")) {
    HandleNetworkDidReceiveDataEvent(base::Value::AsDictionaryValue(val));
  }
}

// Called from page page javascript.
// `function __RECORD_REPLAY_ANNOTATION_HOOK__(kind, contents)`
// Since this function is called from userland JS, we avoid assertions.
// We don't want flawed uses of the API to crash the recording.
static void InvokeOnAnnotation(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (! (args.Length() >= 2 && args[0]->IsString())) {
    recordreplay::Print("%s called with incorrect arguments",
      AnnotationHookJSName);
    return;
  }

  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Object> payload = v8::Object::New(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  payload->Set(context, ToV8String(isolate, "message"), args[1]).Check();

  v8::Local<v8::String> json;
  if (!v8::JSON::Stringify(context, payload).ToLocal(&json)) {
    recordreplay::Print("%s contents failed to json stringify",
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

static void RunScript(v8::Isolate* isolate, v8::Local<v8::Context> context, const char* script, const char* filename) {
  v8::Local<v8::String> filename_string = ToV8String(isolate, filename);
  v8::ScriptOrigin origin(filename_string);

  v8::Local<v8::String> source = ToV8String(isolate, script);

  // TODO: check for errors after `Compile` and `Run`
  // Issue:
  // https://linear.app/replay/issue/RUN-955/chromium-should-not-diverge-and-crash-if-greplayscript-does-not
  v8::Local<v8::Script> compiled = v8::Script::Compile(context, source, &origin).ToLocalChecked();
  compiled->Run(context).ToLocalChecked();
}

static bool TestEnv(const char* env) {
  const char* v = getenv(env);
  return v && v[0] && v[0] != '0';
}


void SetupRecordReplayCommands(v8::Isolate* isolate, LocalFrame* localFrame) {
  V8RecordReplaySetAPIObjectIdCallback(GetAPIObjectIdCallback);
  V8RecordReplayRegisterBrowserEventCallback(HandleBrowserEvent);

  gLocalFrame = localFrame;

  gActiveNetworkRequests =
      new std::unordered_map<std::string, NetworkRequestStatus>();
  gCurrentNetworkStreamData = new std::vector<uint8_t>();

  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  // Add the "__RECORD_REPLAY_ANNOTATION_HOOK__" hook function to
  // the page window global.
  SetFunctionProperty(isolate, context->Global(), AnnotationHookJSName,
                      InvokeOnAnnotation);

  v8::Local<v8::Object> args = v8::Object::New(isolate);
  DefineProperty(isolate, context->Global(), "__RECORD_REPLAY_ARGUMENTS__", args);

  SetFunctionProperty(isolate, args, "log",
                      LogCallback);

  // CDP debugger functionality
  SetFunctionProperty(isolate, args, "setCDPMessageCallback",
                      SetCDPMessageCallback);
  SetFunctionProperty(isolate, args, "sendCDPMessage",
                      SendCDPMessage);
  SetFunctionProperty(isolate, args, "setCommandCallback",
                      v8::FunctionCallbackRecordReplaySetCommandCallback);

  // networking
  SetFunctionProperty(isolate, args, "getCurrentNetworkRequestEvent",
                      GetCurrentNetworkRequestEvent);
  SetFunctionProperty(isolate, args, "getCurrentNetworkStreamData",
                      GetCurrentNetworkStreamData);

  // DOM, blink, API stuff
  // SetFunctionProperty(isolate, args, "jsGetObjectIdForAnyObject",
  //                     jsGetObjectIdForAnyObject);
  // SetFunctionProperty(isolate, args, "jsPreviewBlinkObjectForObjectId", jsPreviewBlinkObjectForObjectId);
  SetFunctionProperty(isolate, args, "fromJsMakeDebuggeeValue",
                      fromJsMakeDebuggeeValue);
  SetFunctionProperty(isolate, args, "fromJsGetObjectByCdpId",
                      fromJsGetObjectByCdpId);

  // unsorted RR stuff
  SetFunctionProperty(
      isolate, args, "setClearPauseDataCallback",
      v8::FunctionCallbackRecordReplaySetClearPauseDataCallback);
  SetFunctionProperty(isolate, args, "getCurrentError",
                      GetCurrentError);
  SetFunctionProperty(isolate, args, "getRecordingId",
                      GetRecordingId);
  SetFunctionProperty(isolate, args, "sha256DigestHex",
                      SHA256DigestHex);
  SetFunctionProperty(isolate, args, "writeToRecordingDirectory",
                      WriteToRecordingDirectory);
  SetFunctionProperty(isolate, args, "addRecordingEvent",
                      AddRecordingEvent);
  SetFunctionProperty(isolate, args, "addNewScriptHandler",
                      v8::FunctionCallbackRecordReplayAddNewScriptHandler);
  SetFunctionProperty(isolate, args, "getScriptSource",
                      v8::FunctionCallbackRecordReplayGetScriptSource);
  SetFunctionProperty(isolate, args, "getPersistentId",
                      GetPersistentId);
  SetFunctionProperty(isolate, args, "checkPersistentId",
                      CheckPersistentId);

  // This URL will prevent the script from being reported to the recorder.
  const char* InternalScriptURL = "record-replay-internal";

  if (recordreplay::FeatureEnabled("collect-source-maps") &&
      !TestEnv("RECORD_REPLAY_DISABLE_SOURCEMAP_COLLECTION")) {
    RunScript(isolate, context, gSourceMapScript, InternalScriptURL);
  }

  if (recordreplay::IsReplaying()) {
    recordreplay::AutoDisallowEvents disallow;
    RunScript(isolate, context, gReplayScript, InternalScriptURL);
  }
}

void RunInitialRecordReplayScripts(v8::Isolate* isolate) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  if (recordreplay::FeatureEnabled("react-devtools-backend") &&
      !TestEnv("RECORD_REPLAY_DISABLE_REACT_DEVTOOLS")) {
    // Note: We use a special URL for the react devtools as this script needs
    // to be reported to the recorder so that evaluations can be performed in
    // its frames.
    RunScript(isolate, context, gReactDevtoolsScript, "record-replay-react-devtools");
  }
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

} // namespace blink
