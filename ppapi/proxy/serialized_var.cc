// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/serialized_var.h"

#include "base/bind.h"
#include "base/logging.h"
#include "ipc/ipc_message_utils.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/dispatcher.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/ppapi_param_traits.h"
#include "ppapi/proxy/ppb_buffer_proxy.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"

namespace ppapi {
namespace proxy {

namespace {
void DefaultHandleWriter(base::Pickle* m, const SerializedHandle& handle) {
  IPC::ParamTraits<SerializedHandle>::Write(m, handle);
}
}  // namespace

// SerializedVar::Inner --------------------------------------------------------

SerializedVar::Inner::Inner()
    : serialization_rules_(nullptr),
      var_(PP_MakeUndefined()),
      instance_(0),
      cleanup_mode_(CLEANUP_NONE),
      is_valid_var_(true) {
#ifndef NDEBUG
  has_been_serialized_ = false;
  has_been_deserialized_ = false;
#endif
}

SerializedVar::Inner::Inner(VarSerializationRules* serialization_rules)
    : serialization_rules_(serialization_rules),
      var_(PP_MakeUndefined()),
      instance_(0),
      cleanup_mode_(CLEANUP_NONE) {
#ifndef NDEBUG
  has_been_serialized_ = false;
  has_been_deserialized_ = false;
#endif
}

SerializedVar::Inner::~Inner() {
  switch (cleanup_mode_) {
    case END_SEND_PASS_REF:
      serialization_rules_->EndSendPassRef(var_);
      break;
    case END_RECEIVE_CALLER_OWNED:
      serialization_rules_->EndReceiveCallerOwned(var_);
      break;
    default:
      break;
  }
}

PP_Var SerializedVar::Inner::GetVar() {
  DCHECK(serialization_rules_.get());

#if defined(NACL_WIN64)
  NOTREACHED();
  return PP_MakeUndefined();
#else
  if (raw_var_data_.get()) {
    var_ = raw_var_data_->CreatePPVar(instance_);
    raw_var_data_.reset(nullptr);
  }

  return var_;
#endif
}

void SerializedVar::Inner::SetVar(PP_Var var) {
  // Sanity check, when updating the var we should have received a
  // serialization rules pointer already.
  DCHECK(serialization_rules_.get());
  var_ = var;
  raw_var_data_.reset(nullptr);
}

void SerializedVar::Inner::SetInstance(PP_Instance instance) {
  instance_ = instance;
}

void SerializedVar::Inner::ForceSetVarValueForTest(PP_Var value) {
  var_ = value;
  raw_var_data_.reset(nullptr);
}

void SerializedVar::Inner::WriteToMessage(base::Pickle* m) const {
  // When writing to the IPC messages, a serialization rules handler should
  // always have been set.
  //
  // When sending a message, it should be difficult to trigger this if you're
  // using the SerializedVarSendInput class and giving a non-null dispatcher.
  // Make sure you're using the proper "Send" helper class.
  //
  // It should be more common to see this when handling an incoming message
  // that returns a var. This means the message handler didn't write to the
  // output parameter, or possibly you used the wrong helper class
  // (normally SerializedVarReturnValue).
  DCHECK(serialization_rules_.get());

#ifndef NDEBUG
  // We should only be serializing something once.
  DCHECK(!has_been_serialized_);
  has_been_serialized_ = true;
#endif
  std::unique_ptr<RawVarDataGraph> data =
      RawVarDataGraph::Create(var_, instance_);
  if (data) {
    m->WriteBool(true);  // Success.
    data->Write(m, base::Bind(&DefaultHandleWriter));
  } else {
    m->WriteBool(false);  // Failure.
  }
}

void SerializedVar::Inner::WriteDataToMessage(
    base::Pickle* m,
    const HandleWriter& handle_writer) const {
  if (raw_var_data_) {
    m->WriteBool(true);  // Success.
    raw_var_data_->Write(m, handle_writer);
  } else {
    m->WriteBool(false);  // Failure.
  }
}

bool SerializedVar::Inner::ReadFromMessage(const base::Pickle* m,
                                           base::PickleIterator* iter) {
#ifndef NDEBUG
  // We should only deserialize something once or will end up with leaked
  // references.
  //
  // One place this has happened in the past is using
  // std::vector<SerializedVar>.resize(). If you're doing this manually instead
  // of using the helper classes for handling in/out vectors of vars, be
  // sure you use the same pattern as the SerializedVarVector classes.
  DCHECK(!has_been_deserialized_);
  has_been_deserialized_ = true;
#endif
  // When reading, the dispatcher should be set when we get a Deserialize
  // call (which will supply a dispatcher).
  if (!iter->ReadBool(&is_valid_var_))
      return false;
  if (is_valid_var_) {
    raw_var_data_ = RawVarDataGraph::Read(m, iter);
    if (!raw_var_data_)
      return false;
  }

  return true;
}

void SerializedVar::Inner::SetCleanupModeToEndSendPassRef() {
  cleanup_mode_ = END_SEND_PASS_REF;
}

void SerializedVar::Inner::SetCleanupModeToEndReceiveCallerOwned() {
  cleanup_mode_ = END_RECEIVE_CALLER_OWNED;
}

// SerializedVar ---------------------------------------------------------------

SerializedVar::SerializedVar() : inner_(new Inner) {
}

SerializedVar::SerializedVar(VarSerializationRules* serialization_rules)
    : inner_(new Inner(serialization_rules)) {
}

SerializedVar::~SerializedVar() {
}

// SerializedVarSendInput ------------------------------------------------------

SerializedVarSendInput::SerializedVarSendInput(Dispatcher* dispatcher,
                                               const PP_Var& var)
    : SerializedVar(dispatcher->serialization_rules()) {
  inner_->SetVar(dispatcher->serialization_rules()->SendCallerOwned(var));
}

// static
void SerializedVarSendInput::ConvertVector(Dispatcher* dispatcher,
                                           const PP_Var* input,
                                           size_t input_count,
                                           std::vector<SerializedVar>* output) {
  output->reserve(input_count);
  for (size_t i = 0; i < input_count; i++)
    output->push_back(SerializedVarSendInput(dispatcher, input[i]));
}

// SerializedVarSendInputShmem -------------------------------------------------

SerializedVarSendInputShmem::SerializedVarSendInputShmem(
    Dispatcher* dispatcher,
    const PP_Var& var,
    const PP_Instance& instance)
    : SerializedVar(dispatcher->serialization_rules()) {
  inner_->SetVar(dispatcher->serialization_rules()->SendCallerOwned(var));
  inner_->SetInstance(instance);
}

// ReceiveSerializedVarReturnValue ---------------------------------------------

ReceiveSerializedVarReturnValue::ReceiveSerializedVarReturnValue() {
}

ReceiveSerializedVarReturnValue::ReceiveSerializedVarReturnValue(
    const SerializedVar& serialized)
    : SerializedVar(serialized) {
}

PP_Var ReceiveSerializedVarReturnValue::Return(Dispatcher* dispatcher) {
  inner_->set_serialization_rules(dispatcher->serialization_rules());
  inner_->SetVar(inner_->serialization_rules()->ReceivePassRef(
      inner_->GetVar()));
  return inner_->GetVar();
}

// ReceiveSerializedException --------------------------------------------------

ReceiveSerializedException::ReceiveSerializedException(Dispatcher* dispatcher,
                                                       PP_Var* exception)
    : SerializedVar(dispatcher->serialization_rules()),
      exception_(exception) {
}

ReceiveSerializedException::~ReceiveSerializedException() {
  if (exception_) {
    // When an output exception is specified, it will take ownership of the
    // reference.
    inner_->SetVar(
        inner_->serialization_rules()->ReceivePassRef(inner_->GetVar()));
    *exception_ = inner_->GetVar();
  } else {
    // When no output exception is specified, the browser thinks we have a ref
    // to an object that we don't want (this will happen only in the plugin
    // since the browser will always specify an out exception for the plugin to
    // write into).
    //
    // Strings don't need this handling since we can just avoid creating a
    // Var from the std::string in the first place.
    if (inner_->GetVar().type == PP_VARTYPE_OBJECT)
      inner_->serialization_rules()->ReleaseObjectRef(inner_->GetVar());
  }
}

bool ReceiveSerializedException::IsThrown() const {
  return exception_ && exception_->type != PP_VARTYPE_UNDEFINED;
}

// ReceiveSerializedVarVectorOutParam ------------------------------------------

ReceiveSerializedVarVectorOutParam::ReceiveSerializedVarVectorOutParam(
    Dispatcher* dispatcher,
    uint32_t* output_count,
    PP_Var** output)
    : dispatcher_(dispatcher),
      output_count_(output_count),
      output_(output) {
}

ReceiveSerializedVarVectorOutParam::~ReceiveSerializedVarVectorOutParam() {
  *output_count_ = static_cast<uint32_t>(vector_.size());
  if (vector_.empty()) {
    *output_ = nullptr;
    return;
  }

  *output_ = static_cast<PP_Var*>(malloc(vector_.size() * sizeof(PP_Var)));
  for (size_t i = 0; i < vector_.size(); i++) {
    // Here we just mimic what happens when returning a value.
    ReceiveSerializedVarReturnValue converted;
    SerializedVar* serialized = &converted;
    *serialized = vector_[i];
    (*output_)[i] = converted.Return(dispatcher_);
  }
}

std::vector<SerializedVar>* ReceiveSerializedVarVectorOutParam::OutParam() {
  return &vector_;
}

// SerializedVarReceiveInput ---------------------------------------------------

SerializedVarReceiveInput::SerializedVarReceiveInput(
    const SerializedVar& serialized)
    : serialized_(serialized) {
}

SerializedVarReceiveInput::~SerializedVarReceiveInput() {
}

PP_Var SerializedVarReceiveInput::Get(Dispatcher* dispatcher) {
  serialized_.inner_->set_serialization_rules(
      dispatcher->serialization_rules());

  // Ensure that when the serialized var goes out of scope it cleans up the
  // stuff we're making in BeginReceiveCallerOwned.
  serialized_.inner_->SetCleanupModeToEndReceiveCallerOwned();

  serialized_.inner_->SetVar(
      serialized_.inner_->serialization_rules()->BeginReceiveCallerOwned(
          serialized_.inner_->GetVar()));
  return serialized_.inner_->GetVar();
}


PP_Var SerializedVarReceiveInput::GetForInstance(Dispatcher* dispatcher,
                                                 PP_Instance instance) {
  serialized_.inner_->SetInstance(instance);
  return Get(dispatcher);
}

// SerializedVarVectorReceiveInput ---------------------------------------------

SerializedVarVectorReceiveInput::SerializedVarVectorReceiveInput(
    const std::vector<SerializedVar>& serialized)
    : serialized_(serialized) {
}

SerializedVarVectorReceiveInput::~SerializedVarVectorReceiveInput() {
  for (size_t i = 0; i < deserialized_.size(); i++) {
    serialized_[i].inner_->serialization_rules()->EndReceiveCallerOwned(
        deserialized_[i]);
  }
}

PP_Var* SerializedVarVectorReceiveInput::Get(Dispatcher* dispatcher,
                                             uint32_t* array_size) {
  deserialized_.resize(serialized_.size());
  for (size_t i = 0; i < serialized_.size(); i++) {
    // The vectors must be able to clean themselves up after this call is
    // torn down.
    serialized_[i].inner_->set_serialization_rules(
        dispatcher->serialization_rules());

    serialized_[i].inner_->SetVar(
        serialized_[i].inner_->serialization_rules()->BeginReceiveCallerOwned(
            serialized_[i].inner_->GetVar()));
    deserialized_[i] = serialized_[i].inner_->GetVar();
  }

  *array_size = static_cast<uint32_t>(serialized_.size());
  return deserialized_.empty() ? nullptr : &deserialized_[0];
}

// SerializedVarReturnValue ----------------------------------------------------

SerializedVarReturnValue::SerializedVarReturnValue(SerializedVar* serialized)
    : serialized_(serialized) {
}

void SerializedVarReturnValue::Return(Dispatcher* dispatcher,
                                      const PP_Var& var) {
  serialized_->inner_->set_serialization_rules(
      dispatcher->serialization_rules());

  // Var must clean up after our BeginSendPassRef call.
  serialized_->inner_->SetCleanupModeToEndSendPassRef();

  serialized_->inner_->SetVar(
      dispatcher->serialization_rules()->BeginSendPassRef(var));
}

// static
SerializedVar SerializedVarReturnValue::Convert(Dispatcher* dispatcher,
                                                const PP_Var& var) {
  // Mimic what happens in the normal case.
  SerializedVar result;
  SerializedVarReturnValue retvalue(&result);
  retvalue.Return(dispatcher, var);
  return result;
}

// SerializedVarOutParam -------------------------------------------------------

SerializedVarOutParam::SerializedVarOutParam(SerializedVar* serialized)
    : serialized_(serialized),
      writable_var_(PP_MakeUndefined()),
      dispatcher_(nullptr) {}

SerializedVarOutParam::~SerializedVarOutParam() {
  if (serialized_->inner_->serialization_rules()) {
    // When unset, OutParam wasn't called. We'll just leave the var untouched
    // in that case.
    serialized_->inner_->SetVar(
        serialized_->inner_->serialization_rules()->BeginSendPassRef(
            writable_var_));

    // Normally the current object will be created on the stack to wrap a
    // SerializedVar and won't have a scope around the actual IPC send. So we
    // need to tell the SerializedVar to do the begin/end send pass ref calls.
    serialized_->inner_->SetCleanupModeToEndSendPassRef();
  }
}

PP_Var* SerializedVarOutParam::OutParam(Dispatcher* dispatcher) {
  dispatcher_ = dispatcher;
  serialized_->inner_->set_serialization_rules(
      dispatcher->serialization_rules());
  return &writable_var_;
}

// SerializedVarVectorOutParam -------------------------------------------------

SerializedVarVectorOutParam::SerializedVarVectorOutParam(
    std::vector<SerializedVar>* serialized)
    : dispatcher_(nullptr),
      serialized_(serialized),
      count_(0),
      array_(nullptr) {}

SerializedVarVectorOutParam::~SerializedVarVectorOutParam() {
  DCHECK(dispatcher_);

  // Convert the array written by the pepper code to the serialized structure.
  // Note we can't use resize here, we have to allocate a new SerializedVar
  // for each serialized item. See ParamTraits<vector<SerializedVar>>::Read.
  serialized_->reserve(count_);
  for (uint32_t i = 0; i < count_; i++) {
    // Just mimic what we do for regular OutParams.
    SerializedVar var;
    SerializedVarOutParam out(&var);
    *out.OutParam(dispatcher_) = array_[i];
    serialized_->push_back(var);
  }

  // When returning arrays, the pepper code expects the caller to take
  // ownership of the array.
  free(array_);
}

PP_Var** SerializedVarVectorOutParam::ArrayOutParam(Dispatcher* dispatcher) {
  DCHECK(!dispatcher_);  // Should only be called once.
  dispatcher_ = dispatcher;
  return &array_;
}

SerializedVarTestConstructor::SerializedVarTestConstructor(
    const PP_Var& pod_var) {
  DCHECK(pod_var.type != PP_VARTYPE_STRING);
  inner_->ForceSetVarValueForTest(pod_var);
}

SerializedVarTestConstructor::SerializedVarTestConstructor(
    const std::string& str) {
  inner_->ForceSetVarValueForTest(StringVar::StringToPPVar(str));
}

SerializedVarTestReader::SerializedVarTestReader(const SerializedVar& var)
    : SerializedVar(var) {
}

}  // namespace proxy
}  // namespace ppapi
