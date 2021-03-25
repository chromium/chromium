// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/v8_schema_registry.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/record_replay.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension_api.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/static_v8_external_one_byte_string_resource.h"
#include "extensions/renderer/v8_helpers.h"

using content::V8ValueConverter;

namespace v8 {

extern void FunctionCallbackRecordReplaySetCommandCallback(const FunctionCallbackInfo<Value>& args);
extern void FunctionCallbackRecordReplaySetClearPauseDataCallback(const FunctionCallbackInfo<Value>& callArgs);
extern void FunctionCallbackRecordReplayIgnoreScript(const FunctionCallbackInfo<Value>& args);

} // namespace v8

namespace extensions {

namespace {

// Recursively freezes every v8 object on |object|.
void DeepFreeze(const v8::Local<v8::Object>& object,
                const v8::Local<v8::Context>& context) {
  // Don't let the object trace upwards via the prototype.
  v8::Maybe<bool> maybe =
      object->SetPrototype(context, v8::Null(context->GetIsolate()));
  CHECK(maybe.IsJust() && maybe.FromJust());
  v8::Local<v8::Array> property_names =
      object->GetOwnPropertyNames(context).ToLocalChecked();
  for (uint32_t i = 0; i < property_names->Length(); ++i) {
    v8::Local<v8::Value> child =
        object->Get(context, property_names->Get(context, i).ToLocalChecked())
            .ToLocalChecked();
    if (child->IsObject())
      DeepFreeze(v8::Local<v8::Object>::Cast(child), context);
  }
  object->SetIntegrityLevel(context, v8::IntegrityLevel::kFrozen);
}

class SchemaRegistryNativeHandler : public ObjectBackedNativeHandler {
 public:
  SchemaRegistryNativeHandler(V8SchemaRegistry* registry,
                              std::unique_ptr<ScriptContext> context)
      : ObjectBackedNativeHandler(context.get()),
        context_(std::move(context)),
        registry_(registry) {}

  // ObjectBackedNativeHandler:
  void AddRoutes() override {
    RouteHandlerFunction(
        "GetSchema",
        base::BindRepeating(&SchemaRegistryNativeHandler::GetSchema,
                            base::Unretained(this)));
    RouteHandlerFunction(
        "GetObjectType",
        base::BindRepeating(&SchemaRegistryNativeHandler::GetObjectType,
                            base::Unretained(this)));
  }

  ~SchemaRegistryNativeHandler() override { context_->Invalidate(); }

 private:
  void GetSchema(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(registry_->GetSchema(
        *v8::String::Utf8Value(args.GetIsolate(), args[0])));
  }

  void GetObjectType(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK(args.Length() == 1 && args[0]->IsObject());
    std::string type;
    if (args[0]->IsArray())
      type = "array";
    else if (args[0]->IsArrayBuffer() || args[0]->IsArrayBufferView())
      type = "binary";
    else
      type = "object";
    args.GetReturnValue().Set(
        v8_helpers::ToV8StringUnsafe(context()->isolate(), type.c_str()));
  }

  std::unique_ptr<ScriptContext> context_;
  V8SchemaRegistry* registry_;
};

}  // namespace

V8SchemaRegistry::V8SchemaRegistry() {
}

V8SchemaRegistry::~V8SchemaRegistry() {
}

std::unique_ptr<NativeHandler> V8SchemaRegistry::AsNativeHandler() {
  std::unique_ptr<ScriptContext> context(
      new ScriptContext(GetOrCreateContext(v8::Isolate::GetCurrent()),
                        NULL,  // no frame
                        NULL,  // no extension
                        Feature::UNSPECIFIED_CONTEXT,
                        NULL,  // no effective extension
                        Feature::UNSPECIFIED_CONTEXT));
  return std::unique_ptr<NativeHandler>(
      new SchemaRegistryNativeHandler(this, std::move(context)));
}

v8::Local<v8::Array> V8SchemaRegistry::GetSchemas(
    const std::vector<std::string>& apis) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetOrCreateContext(isolate);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Array> v8_apis(v8::Array::New(isolate, apis.size()));
  size_t api_index = 0;
  for (auto i = apis.cbegin(); i != apis.cend(); ++i) {
    bool set_result =
        v8_apis->Set(context, api_index++, GetSchema(*i)).ToChecked();
    // Set() should never return false without throwing an exception (which
    // would be caught by the ToChecked() above).
    DCHECK(set_result);
  }
  return handle_scope.Escape(v8_apis);
}

v8::Local<v8::Object> V8SchemaRegistry::GetSchema(const std::string& api) {
  if (schema_cache_ != NULL) {
    v8::Local<v8::Object> cached_schema = schema_cache_->Get(api);
    if (!cached_schema.IsEmpty()) {
      return cached_schema;
    }
  }

  // Slow path: Need to build schema first.

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetOrCreateContext(isolate);
  v8::Context::Scope context_scope(context);

  base::StringPiece schema_string =
      ExtensionAPI::GetSharedInstance()->GetSchemaStringPiece(api);
  CHECK(!schema_string.empty());
  v8::MaybeLocal<v8::String> v8_maybe_string = v8::String::NewExternalOneByte(
      isolate, new StaticV8ExternalOneByteStringResource(schema_string));
  v8::Local<v8::String> v8_schema_string;
  CHECK(v8_maybe_string.ToLocal(&v8_schema_string));

  v8::MaybeLocal<v8::Value> v8_maybe_schema_value =
      v8::JSON::Parse(context, v8_schema_string);
  v8::Local<v8::Value> v8_schema_value;
  CHECK(v8_maybe_schema_value.ToLocal(&v8_schema_value));
  CHECK(v8_schema_value->IsObject());

  v8::Local<v8::Object> v8_schema_object(
      v8::Local<v8::Object>::Cast(v8_schema_value));
  DeepFreeze(v8_schema_object, context);

  schema_cache_->Set(api, v8_schema_object);

  return handle_scope.Escape(v8_schema_object);
}

const char* gRecordReplayScript = R""""(

const {
  log,
  setCDPMessageCallback,
  sendCDPMessage,
  setCommandCallback,
  setClearPauseDataCallback,
  ignoreScript,
} = __RECORD_REPLAY_ARGUMENTS__;

try {

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
  sendCDPMessage(JSON.stringify({ method, params, id }));
  gCurrentMessageId = undefined;
  return gCurrentMessageResult;
}

const gEventListeners = new Map();

function addEventListener(method, callback) {
  gEventListeners.set(method, callback);
}

function messageCallback(message) {
  try {
    message = JSON.parse(message);

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
  "Target.countStackFrames": Target_countStackFrames,
  "Target.getCurrentMessageContents": Target_getCurrentMessageContents,
  "Target.getSourceMapURL": Target_getSourceMapURL,
  "Target.getStepOffsets": Target_getStepOffsets,
  "Target.topFrameLocation": Target_topFrameLocation,
  "Pause.evaluateInFrame": Pause_evaluateInFrame,
  "Pause.getAllFrames": Pause_getAllFrames,
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

function Target_countStackFrames() {
  const count = getStackFrames().length;
  return { count };
}

// Contents of the last console API call. Runtime.consoleAPICalled will be
// emitted before the driver gets the current message contents.
let gLastConsoleAPICall;

function onConsoleAPICall(params) {
  gLastConsoleAPICall = params;
}

function Target_getCurrentMessageContents() {
  // Look for the "args" variable on an onConsoleMessage frame.
  // The arguments are also stored on the last console API call, though
  // if we use that we need to be careful because the pause state could have
  // been cleared since the last Runtime.consoleAPICalled event.
  const { callFrames } = sendMessage("Debugger.getCallFrames");
  const consoleMessageFrame = callFrames.find(
    frame => frame.functionName == "onConsoleMessage"
  );
  assert(consoleMessageFrame);
  assert(consoleMessageFrame.this.type == "object");
  assert(consoleMessageFrame.this.className == "Array");
  const argumentsId = consoleMessageFrame.this.objectId;

  // Get the properties of the message arguments array.
  const argumentsProperties = sendMessage("Runtime.getProperties", {
    objectId: argumentsId,
    ownProperties: true,
    generatePreview: false,
  }).result;

  // Get the protocol representation of the message arguments.
  const argumentValues = [];
  for (let i = 0;; i++) {
    const property = argumentsProperties.find(prop => prop.name == i.toString());
    if (!property) {
      break;
    }
    argumentValues.push(remoteObjectToProtocolValue(property.value));
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
  gProtocolIdToObject.clear();
  gObjectIdToProtocolId.clear();
  gProtocolIdToScope.clear();
  gNextObjectId = 1;
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
  const className = obj.className || "Function";

  let preview;
  if (level != "none") {
    preview = new ProtocolObjectPreview(obj, level).fill();
  }

  return { objectId, className, preview };
}

// Note: this is higher than on gecko-dev because typed arrays don't render
// properly in the devtools currently unless we include a minimum number of
// properties. This would be nice to fix.
const NumItemsBeforeOverflow = 12;

function ProtocolObjectPreview(obj, level) {
  this.obj = obj;
  this.level = level;
  this.overflow = false;
  this.numItems = 0;
  this.extra = {};
}

ProtocolObjectPreview.prototype = {
  canAddItem(force) {
    if (this.level == "noProperties") {
      this.overflow = true;
      return false;
    }
    if (!force && this.level == "canOverflow" && this.numItems >= NumItemsBeforeOverflow) {
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
      overflow: this.overflow ? true : undefined,
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
  const { name, value, writable, get, set, configurable, enumerable } = desc;

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

  v8::Local<v8::String> name_string =
    v8::String::NewFromUtf8(isolate, name,
                            v8::NewStringType::kInternalized).ToLocalChecked();

  obj->Set(context, name_string, function).Check();
  function->SetName(name_string);
}

static void RecordReplayLog(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() &&
        "must be called with a single string");
  v8::String::Utf8Value text(args.GetIsolate(), args[0]);
  recordreplay::Print("%s", *text);
}

static void RecordReplaySetCDPMessageCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  recordreplay::Print("CALL_SET_CDP_MESSAGE_CALLBACK");
}

static void RecordReplaySendCDPMessage(const v8::FunctionCallbackInfo<v8::Value>& args) {
  recordreplay::Print("CALL_SEND_CDP_MESSAGE");
}

static void SetupRecordReplayCommands(v8::Isolate* isolate) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::String> args_name_string =
    v8::String::NewFromUtf8(isolate, "__RECORD_REPLAY_ARGUMENTS__",
                            v8::NewStringType::kInternalized).ToLocalChecked();

  v8::Local<v8::Object> args = v8::Object::New(isolate);
  context->Global()->Set(context, args_name_string, args).Check();

  SetFunctionProperty(isolate, args, "log",
                      RecordReplayLog);
  SetFunctionProperty(isolate, args, "setCDPMessageCallback",
                      RecordReplaySetCDPMessageCallback);
  SetFunctionProperty(isolate, args, "sendCDPMessage",
                      RecordReplaySendCDPMessage);
  SetFunctionProperty(isolate, args, "setCommandCallback",
                      v8::FunctionCallbackRecordReplaySetCommandCallback);
  SetFunctionProperty(isolate, args, "setClearPauseDataCallback",
                      v8::FunctionCallbackRecordReplaySetClearPauseDataCallback);
  SetFunctionProperty(isolate, args, "ignoreScript",
                      v8::FunctionCallbackRecordReplayIgnoreScript);

  v8::Local<v8::String> source =
    v8::String::NewFromUtf8(isolate, gRecordReplayScript,
                            v8::NewStringType::kInternalized).ToLocalChecked();

  v8::Local<v8::String> filename =
    v8::String::NewFromUtf8(isolate, "record-replay-internal",
                            v8::NewStringType::kInternalized).ToLocalChecked();

  v8::ScriptOrigin origin(filename);
  v8::Local<v8::Script> script = v8::Script::Compile(context, source, &origin).ToLocalChecked();
  script->Run(context).ToLocalChecked();
}

static bool gHasContext;

v8::Local<v8::Context> V8SchemaRegistry::GetOrCreateContext(
    v8::Isolate* isolate) {
  // It's ok to create local handles in this function, since this is only called
  // when we have a HandleScope.
  if (!context_holder_) {
    context_holder_.reset(new gin::ContextHolder(isolate));
    context_holder_->SetContext(v8::Context::New(isolate));
    schema_cache_.reset(new SchemaCache(isolate));

    // After creating the first context, we are ready to set up the state used
    // to process driver commands when recording/replaying, and to create
    // checkpoints. Create the first checkpoint at which execution can pause.
    if (!gHasContext) {
      gHasContext = true;
      SetupRecordReplayCommands(isolate);
      recordreplay::NewCheckpoint();
    }

    return context_holder_->context();
  }
  return context_holder_->context();
}

}  // namespace extensions
