// Copyright 2021 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/record_replay_interface.h"

#include "base/record_replay.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "v8/include/v8-inspector.h"

namespace v8 {

extern void FunctionCallbackRecordReplaySetCommandCallback(const FunctionCallbackInfo<Value>& args);
extern void FunctionCallbackRecordReplaySetClearPauseDataCallback(const FunctionCallbackInfo<Value>& callArgs);
extern void FunctionCallbackRecordReplayIgnoreScript(const FunctionCallbackInfo<Value>& args);

} // namespace v8

namespace blink {

const char* gRecordReplayScript = R""""(

const {
  log,
  setCDPMessageCallback,
  sendCDPMessage,
  setCommandCallback,
  setClearPauseDataCallback,
  getCurrentError,
  notifyDriverOnConsoleAPICall,
  ignoreScript,
  dump,
} = __RECORD_REPLAY_ARGUMENTS__;

try {

window.dump = dump;

// Save these before page code potentially overwrites them.
const JSON_stringify = JSON.stringify;
const JSON_parse = JSON.parse;

///////////////////////////////////////////////////////////////////////////////
// utils.js
///////////////////////////////////////////////////////////////////////////////

function assert(v) {
  if (!v) {
    log(`Assertion failed ${Error().stack}`);
    throw new Error("Assertion failed");
  }
}

///////////////////////////////////////////////////////////////////////////////
// message.js
///////////////////////////////////////////////////////////////////////////////

function initMessages() {
  setCDPMessageCallback(messageCallback);
}

let gNextMessageId = 1;

let gCurrentMessageId;
let gCurrentMessageResult;

function sendMessage(method, params) {
  const id = gNextMessageId++;
  gCurrentMessageId = id;
  sendCDPMessage(JSON_stringify({ method, params, id }));
  gCurrentMessageId = undefined;
  return gCurrentMessageResult;
}

const gEventListeners = new Map();

function addEventListener(method, callback) {
  gEventListeners.set(method, callback);
}

function messageCallback(message) {
  try {
    message = JSON_parse(message);

    if (message.id) {
      assert(message.id == gCurrentMessageId);
      gCurrentMessageResult = message.result;
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
setCommandCallback(commandCallback);
setClearPauseDataCallback(clearPauseDataCallback);
addEventListener("Runtime.consoleAPICalled", onConsoleAPICall);
sendMessage("Runtime.enable");

const CommandCallbacks = {
  "Target.getCurrentMessageContents": Target_getCurrentMessageContents,
  "Target.getSourceMapURL": Target_getSourceMapURL,
  "Target.getStepOffsets": Target_getStepOffsets,
  "Target.topFrameLocation": Target_topFrameLocation,
  "Pause.evaluateInFrame": Pause_evaluateInFrame,
  "Pause.evaluateInGlobal": Pause_evaluateInGlobal,
  "Pause.getAllFrames": Pause_getAllFrames,
  "Pause.getExceptionValue": Pause_getExceptionValue,
  "Pause.getObjectPreview": Pause_getObjectPreview,
  "Pause.getObjectProperty": Pause_getObjectProperty,
  "Pause.getScope": Pause_getScope,
};

function commandCallback(method, params) {
  if (!CommandCallbacks[method]) {
    log(`Missing command callback: ${method}`);
    return {};
  }

  try {
    return CommandCallbacks[method](params);
  } catch (e) {
    log(`Error: Command exception ${method} ${e}`);
    return {};
  }
}

// Contents of the last console API call. Runtime.consoleAPICalled will be
// emitted before the driver gets the current message contents.
let gLastConsoleAPICall;

function onConsoleAPICall(params) {
  gLastConsoleAPICall = params;
  notifyDriverOnConsoleAPICall();
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
    argumentValues.push(remoteObjectToProtocolValue(arg));
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

function Target_getSourceMapURL() {
  // NYI
  return {};
}

function Target_getStepOffsets() {
  // CDP does not distinguish between steps and breakpoints.
  return {};
}

function Target_topFrameLocation() {
  const frames = getStackFrames();
  if (!frames.length) {
    return {};
  }
  return { location: createProtocolLocation(frames[0].location)[0] };
}

// Get the raw call frames on the stack, eliding ones in scripts we are ignoring.
function getStackFrames() {
  const { callFrames } = sendMessage("Debugger.getCallFrames");

  const frames = [];
  for (const frame of callFrames) {
    if (!ignoreScript(frame.location.scriptId)) {
      frames.push(frame);
    }
  }
  return frames;
}

// Build a protocol Result object from a result/exceptionDetails CDP rval.
function buildProtocolResult({ result, exceptionDetails }) {
  const value = remoteObjectToProtocolValue(result);
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
  return buildProtocolResult(rv);

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
  return buildProtocolResult(rv);
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
  const rv = sendMessage("Debugger.getPendingException", {});
  return { exception: remoteObjectToProtocolValue(rv.exception), data: {} };
}

function Pause_getObjectPreview({ object, level = "full" }) {
  const objectData = createProtocolObject(object, level);
  return { data: { objects: [objectData] } };
}

function Pause_getObjectProperty({ object, name }) {
  const obj = protocolIdToRemoteObject(object);
  const rv = sendMessage(
    "Runtime.callFunctionOn",
    {
      functionDeclaration: `function() { return this["${name}"] }`,
      objectId: obj.objectId,
    }
  );
  return buildProtocolResult(rv);
}

function Pause_getScope({ scope }) {
  const scopeData = createProtocolScope(scope);
  return { data: { scopes: [scopeData] } };
}

///////////////////////////////////////////////////////////////////////////////
// object.js
///////////////////////////////////////////////////////////////////////////////

// Manage association between remote objects and protocol object IDs.

// Map protocol ObjectId => RemoteObject
const gProtocolIdToObject = new Map();

// Map RemoteObject.objectId => protocol ObjectId
const gObjectIdToProtocolId = new Map();

// Map protocol ScopeId => Debugger.Scope
const gProtocolIdToScope = new Map();

let gNextObjectId = 1;

function clearPauseDataCallback() {
  try {
    gProtocolIdToObject.clear();
    gObjectIdToProtocolId.clear();
    gProtocolIdToScope.clear();
    gNextObjectId = 1;
  } catch (e) {
    log(`Error: clearPauseDataCallback exception: ${e}`);
  }
}

function remoteObjectToProtocolId(remoteObject) {
  assert(remoteObject.objectId);

  const existing = gObjectIdToProtocolId.get(remoteObject.objectId);
  if (existing) {
    return existing;
  }

  const protocolObjectId = (gNextObjectId++).toString();
  gObjectIdToProtocolId.set(remoteObject.objectId, protocolObjectId);
  gProtocolIdToObject.set(protocolObjectId, remoteObject);

  return protocolObjectId;
}

function protocolIdToRemoteObject(objectId) {
  const remoteObject = gProtocolIdToObject.get(objectId);
  assert(remoteObject);
  return remoteObject;
}

// Strings longer than this will be truncated when creating protocol values.
const MaxStringLength = 10000;

function remoteObjectToProtocolValue(obj) {
  switch (obj.type) {
    case "undefined":
      return {};
    case "string":
    case "number":
    case "boolean":
      if (obj.unserializableValue) {
        assert(obj.type == "number");
        return { unserializableNumber: obj.unserializableValue };
      }
      if (typeof obj.value == "string" && obj.value.length > MaxStringLength) {
        return { value: obj.value.substring(0, MaxStringLength) + "…" };
      }
      return { value: obj.value };
    case "bigint": {
      const str = obj.unserializableValue;
      assert(str);
      return { bigint: str.substring(0, str.length - 1) };
    }
    case "object":
    case "function": {
      if (!obj.objectId) {
        return { value: null };
      }
      const object = remoteObjectToProtocolId(obj);
      return { object };
    }
    case "symbol":
      return { symbol: obj.description };
    default:
      return { unavailable: true };
  }
}

function scopeToProtocolId(scope) {
  // Use the scope object's ID as the ID for the scope itself.
  const id = remoteObjectToProtocolId(scope.object);
  gProtocolIdToScope.set(id, scope);
  return id;
}

function protocolIdToScope(scopeId) {
  const scope = gProtocolIdToScope.get(scopeId);
  assert(scope);
  return scope;
}

///////////////////////////////////////////////////////////////////////////////
// preview.js
///////////////////////////////////////////////////////////////////////////////

// Logic for creating object previews for the record/replay protocol.

function createProtocolObject(objectId, level) {
  const obj = protocolIdToRemoteObject(objectId);
  const className = obj.subtype == "proxy" ? "Proxy" : (obj.className || "Function");

  let preview;
  if (level != "none") {
    preview = new ProtocolObjectPreview(obj, level).fill();
  }

  return { objectId, className, preview };
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
  this.obj = obj;
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
    const allProperties = sendMessage("Runtime.getProperties", {
      objectId: this.obj.objectId,
      ownProperties: true,
      generatePreview: false,
    });
    const properties = allProperties.result;

    // Add class-specific data.
    const previewer = CustomPreviewers[this.obj.className];
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

    let prototypeId;
    if (prototype && prototype.value && prototype.value.objectId) {
      prototypeId = remoteObjectToProtocolId(prototype.value);
    }
    return {
      prototypeId,
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
          key: key ? remoteObjectToProtocolValue(key.value) : undefined,
          value: remoteObjectToProtocolValue(value.value),
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
  this.extra.dateTime = Date.parse(this.obj.description);
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

  const rv = value ? remoteObjectToProtocolValue(value) : {};
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
    rv.get = remoteObjectToProtocolId(get);
  }
  if (set && set.objectId) {
    rv.set = remoteObjectToProtocolId(set);
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

function createProtocolFrame(frameId, frame) {
  // CDP call frames don't provide detailed type information.
  const type = frame.functionName ? "call" : "global";

  return {
    frameId,
    type,
    functionName: frame.functionName || undefined,
    functionLocation: createProtocolLocation(frame.functionLocation),
    location: createProtocolLocation(frame.location),
    scopeChain: frame.scopeChain.map(scopeToProtocolId),
    this: remoteObjectToProtocolValue(frame.this),
  };
}

function createProtocolScope(scopeId) {
  const scope = protocolIdToScope(scopeId);

  let type;
  switch (scope.type) {
    case "global":
      type = "global";
      break;
    case "with":
      type = "with";
      break;
    default:
      type = scope.name ? "function" : "block";
      break;
  }

  let object, bindings;
  if (type == "global" || type == "with") {
    object = remoteObjectToProtocolId(scope.object);
  } else {
    bindings = [];

    const properties = sendMessage("Runtime.getProperties", {
      objectId: scope.object.objectId,
      ownProperties: true,
      generatePreview: false,
    }).result;
    for (const { name, value } of properties) {
      const converted = remoteObjectToProtocolValue(value);
      bindings.push({ ...converted, name });
    }
  }

  return {
    scopeId,
    type,
    object,
    functionName: scope.name || undefined,
    bindings,
  };
}

} catch (e) {
  log(`Error: Initialization exception ${e}`);
}

)"""";

static v8::Local<v8::String> ToV8String(v8::Isolate* isolate, const char* value) {
  return v8::String::NewFromUtf8(isolate, value,
                                 v8::NewStringType::kInternalized).ToLocalChecked();
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

  v8::Local<v8::String> name_string = ToV8String(isolate, name);
  obj->Set(context, name_string, function).Check();
  function->SetName(name_string);
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

void RecordReplayRegisterV8Inspector(v8_inspector::V8Inspector* inspector) {
  if (v8::IsMainThread()) {
    gInspector = inspector;

    // For now we only connect to the first frame.
    static int ContextGroupId = 1;

    gInspectorSession = gInspector->connect(ContextGroupId,
                                            new InspectorChannel(),
                                            v8_inspector::StringView()).release();
  }
}

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

static void GetCurrentError(const v8::FunctionCallbackInfo<v8::Value>& args);

extern "C" void V8RecordReplayOnConsoleMessage(size_t bookmark);

static void NotifyDriverOnConsoleAPICall(const v8::FunctionCallbackInfo<v8::Value>& args) {
  V8RecordReplayOnConsoleMessage(0);
}

extern "C" void V8RecordReplayFinishRecording();

// Mimic the gecko test runner behavior when using window.dump() to signal that the
// recording is finished. This is pretty hacky.
static void DumpCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() == 1 && args[0]->IsString()) {
    v8::String::Utf8Value message(args.GetIsolate(), args[0]);
    if (!strcmp(*message, "RecReplaySendAsyncMessage Example__Finished")) {
      V8RecordReplayFinishRecording();
    }
  }
}

// When JS assertions are enabled, this callback is used to get any pointer ID
// associated with a given API object.
static int GetAPIObjectIdCallback(v8::Local<v8::Object> object) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  const WrapperTypeInfo* infos[] = {
    // ScriptWrappable itself doesn't have wrapper type info, so check subclasses.
    Node::GetStaticWrapperTypeInfo(),
    Event::GetStaticWrapperTypeInfo(),
  };
  for (const WrapperTypeInfo* info : infos) {
    if (V8PerIsolateData::From(isolate)->HasInstance(info, object)) {
      ScriptWrappable* wrappable = ToScriptWrappable(object);
      int id = recordreplay::PointerId(wrappable);
      CHECK(id);
      return id;
    }
  }
  return 0;
}

extern "C" void V8RecordReplaySetAPIObjectIdCallback(int (*callback)(v8::Local<v8::Object>));

void SetupRecordReplayCommands(v8::Isolate* isolate) {
  V8RecordReplaySetAPIObjectIdCallback(GetAPIObjectIdCallback);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::String> args_name_string =
    ToV8String(isolate, "__RECORD_REPLAY_ARGUMENTS__");

  v8::Local<v8::Object> args = v8::Object::New(isolate);
  context->Global()->Set(context, args_name_string, args).Check();

  SetFunctionProperty(isolate, args, "log",
                      LogCallback);
  SetFunctionProperty(isolate, args, "setCDPMessageCallback",
                      SetCDPMessageCallback);
  SetFunctionProperty(isolate, args, "sendCDPMessage",
                      SendCDPMessage);
  SetFunctionProperty(isolate, args, "setCommandCallback",
                      v8::FunctionCallbackRecordReplaySetCommandCallback);
  SetFunctionProperty(isolate, args, "setClearPauseDataCallback",
                      v8::FunctionCallbackRecordReplaySetClearPauseDataCallback);
  SetFunctionProperty(isolate, args, "ignoreScript",
                      v8::FunctionCallbackRecordReplayIgnoreScript);
  SetFunctionProperty(isolate, args, "getCurrentError",
                      GetCurrentError);
  SetFunctionProperty(isolate, args, "notifyDriverOnConsoleAPICall",
                      NotifyDriverOnConsoleAPICall);
  SetFunctionProperty(isolate, args, "dump",
                      DumpCallback);

  v8::Local<v8::String> source = ToV8String(isolate, gRecordReplayScript);
  v8::Local<v8::String> filename = ToV8String(isolate, "record-replay-internal");

  v8::ScriptOrigin origin(filename);
  v8::Local<v8::Script> script = v8::Script::Compile(context, source, &origin).ToLocalChecked();
  script->Run(context).ToLocalChecked();
}

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

static void SetDataProperty(v8::Isolate* isolate, v8::Local<v8::Object> obj,
                            const char* property, v8::Local<v8::Value> value) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  obj->Set(context, ToV8String(isolate, property), value).Check();
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
