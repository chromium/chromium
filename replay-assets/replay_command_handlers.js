(() => {
// Script which defines handlers for recorder commands, 
// and usually is only loaded while replaying.
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
