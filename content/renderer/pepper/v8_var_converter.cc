// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/v8_var_converter.h"

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/containers/stack.h"
#include "base/location.h"
#include "base/logging.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/host_array_buffer_var.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/resource_converter.h"
#include "content/renderer/pepper/v8object_var.h"
#include "ppapi/shared_impl/array_var.h"
#include "ppapi/shared_impl/dictionary_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "third_party/blink/public/web/web_array_buffer.h"
#include "third_party/blink/public/web/web_array_buffer_converter.h"

using ppapi::ArrayBufferVar;
using ppapi::ArrayVar;
using ppapi::DictionaryVar;
using ppapi::ScopedPPVar;
using ppapi::StringVar;
using ppapi::V8ObjectVar;
using std::make_pair;

namespace {

template <class T>
struct StackEntry {
  StackEntry(T v) : val(v), sentinel(false) {}
  T val;
  // Used to track parent nodes on the stack while traversing the graph.
  bool sentinel;
};

struct HashedHandle {
  HashedHandle(v8::Local<v8::Object> h) : handle(h) {}
  size_t hash() const { return handle->GetIdentityHash(); }
  bool operator==(const HashedHandle& h) const { return handle == h.handle; }
  v8::Local<v8::Object> handle;
};

}  // namespace

namespace std {
template <>
struct hash<HashedHandle> {
  size_t operator()(const HashedHandle& handle) const { return handle.hash(); }
};
}  // namespace std

namespace content {

namespace {

// Maps PP_Var IDs to the V8 value handle they correspond to.

typedef std::unordered_map<int64_t, v8::Local<v8::Value>> VarHandleMap;
typedef std::unordered_set<int64_t> ParentVarSet;

// Maps V8 value handles to the PP_Var they correspond to.
typedef std::unordered_map<HashedHandle, ScopedPPVar> HandleVarMap;
typedef std::unordered_set<HashedHandle> ParentHandleSet;

// Returns a V8 value which corresponds to a given PP_Var. If |var| is a
// reference counted PP_Var type, and it exists in |visited_ids|, the V8 value
// associated with it in the map will be returned, otherwise a new V8 value will
// be created and added to the map. |did_create| indicates whether a new v8
// value was created as a result of calling the function.
bool GetOrCreateV8Value(v8::Local<v8::Context> context,
                        const PP_Var& var,
                        V8VarConverter::AllowObjectVars object_vars_allowed,
                        v8::Local<v8::Value>* result,
                        bool* did_create,
                        VarHandleMap* visited_ids,
                        ParentVarSet* parent_ids,
                        ResourceConverter* resource_converter) {
  v8::Isolate* isolate = context->GetIsolate();
  *did_create = false;

  if (ppapi::VarTracker::IsVarTypeRefcounted(var.type)) {
    if (parent_ids->count(var.value.as_id) != 0)
      return false;
    auto it = visited_ids->find(var.value.as_id);
    if (it != visited_ids->end()) {
      *result = it->second;
      return true;
    }
  }

  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      *result = v8::Undefined(isolate);
      break;
    case PP_VARTYPE_NULL:
      *result = v8::Null(isolate);
      break;
    case PP_VARTYPE_BOOL:
      *result = (var.value.as_bool == PP_TRUE) ? v8::True(isolate)
                                               : v8::False(isolate);
      break;
    case PP_VARTYPE_INT32:
      *result = v8::Integer::New(isolate, var.value.as_int);
      break;
    case PP_VARTYPE_DOUBLE:
      *result = v8::Number::New(isolate, var.value.as_double);
      break;
    case PP_VARTYPE_STRING: {
      StringVar* string = StringVar::FromPPVar(var);
      if (!string) {
        NOTREACHED();
        result->Clear();
        return false;
      }
      const std::string& value = string->value();
      // Create a string primitive rather than a string object. This is lossy
      // in the sense that string primitives in JavaScript can't be referenced
      // in the same way that string vars can in pepper. But that information
      // isn't very useful and primitive strings are a more expected form in JS.
      *result =
          v8::String::NewFromUtf8(isolate, value.c_str(),
                                  v8::NewStringType::kNormal, value.size())
              .ToLocalChecked();
      break;
    }
    case PP_VARTYPE_ARRAY_BUFFER: {
      ArrayBufferVar* buffer = ArrayBufferVar::FromPPVar(var);
      if (!buffer) {
        NOTREACHED();
        result->Clear();
        return false;
      }
      HostArrayBufferVar* host_buffer =
          static_cast<HostArrayBufferVar*>(buffer);
      *result = blink::WebArrayBufferConverter::ToV8Value(
          &host_buffer->webkit_buffer(), context->Global(), isolate);
      break;
    }
    case PP_VARTYPE_ARRAY:
      *result = v8::Array::New(isolate);
      break;
    case PP_VARTYPE_DICTIONARY:
      *result = v8::Object::New(isolate);
      break;
    case PP_VARTYPE_OBJECT: {
      // If object vars are disallowed, we should never be passed an object var
      // to convert. Also, we should never expect to convert an object var which
      // is nested inside an array or dictionary.
      if (object_vars_allowed == V8VarConverter::kDisallowObjectVars ||
          visited_ids->size() != 0) {
        NOTREACHED();
        result->Clear();
        return false;
      }
      scoped_refptr<V8ObjectVar> v8_object_var = V8ObjectVar::FromPPVar(var);
      if (!v8_object_var.get()) {
        NOTREACHED();
        result->Clear();
        return false;
      }
      *result = v8_object_var->GetHandle();
      break;
    }
    case PP_VARTYPE_RESOURCE:
      if (!resource_converter->ToV8Value(var, context, result)) {
        result->Clear();
        return false;
      }
      break;
  }

  *did_create = true;
  if (ppapi::VarTracker::IsVarTypeRefcounted(var.type))
    (*visited_ids)[var.value.as_id] = *result;
  return true;
}

// For a given V8 value handle, this returns a PP_Var which corresponds to it.
// If the handle already exists in |visited_handles|, the PP_Var associated with
// it will be returned, otherwise a new V8 value will be created and added to
// the map. |did_create| indicates if a new PP_Var was created as a result of
// calling the function.
bool GetOrCreateVar(v8::Local<v8::Value> val,
                    v8::Local<v8::Context> context,
                    PP_Instance instance,
                    V8VarConverter::AllowObjectVars object_vars_allowed,
                    PP_Var* result,
                    bool* did_create,
                    HandleVarMap* visited_handles,
                    ParentHandleSet* parent_handles,
                    ResourceConverter* resource_converter) {
  CHECK(!val.IsEmpty());
  *did_create = false;

  v8::Isolate* isolate = context->GetIsolate();
  // Even though every v8 string primitive encountered will be a unique object,
  // we still add them to |visited_handles| so that the corresponding string
  // PP_Var created will be properly refcounted.
  if (val->IsObject() || val->IsString()) {
    if (parent_handles->count(
            HashedHandle(val->ToObject(context).ToLocalChecked())) != 0)
      return false;

    HandleVarMap::const_iterator it = visited_handles->find(
        HashedHandle(val->ToObject(context).ToLocalChecked()));
    if (it != visited_handles->end()) {
      *result = it->second.get();
      return true;
    }
  }

  if (val->IsUndefined()) {
    *result = PP_MakeUndefined();
  } else if (val->IsNull()) {
    *result = PP_MakeNull();
  } else if (val->IsBoolean() || val->IsBooleanObject()) {
    *result = PP_MakeBool(PP_FromBool(val->ToBoolean(isolate)->Value()));
  } else if (val->IsInt32()) {
    *result = PP_MakeInt32(val.As<v8::Int32>()->Value());
  } else if (val->IsNumber() || val->IsNumberObject()) {
    *result = PP_MakeDouble(val->NumberValue(context).ToChecked());
  } else if (val->IsString() || val->IsStringObject()) {
    v8::String::Utf8Value utf8(isolate,
                               val->ToString(context).ToLocalChecked());
    *result = StringVar::StringToPPVar(std::string(*utf8, utf8.length()));
  } else if (val->IsObject()) {
    // For any other v8 objects, the conversion happens as follows:
    // 1) If the object is an array buffer, return an ArrayBufferVar.
    // 2) If object vars are allowed, return the object wrapped as a
    //    V8ObjectVar. This is to maintain backward compatibility with
    //    synchronous scripting in Flash.
    // 3) If the object is an array, return an ArrayVar.
    // 4) If the object can be converted to a resource, return the ResourceVar.
    // 5) Otherwise return a DictionaryVar.
    std::unique_ptr<blink::WebArrayBuffer> web_array_buffer(
        blink::WebArrayBufferConverter::CreateFromV8Value(val, isolate));
    if (web_array_buffer.get()) {
      scoped_refptr<HostArrayBufferVar> buffer_var(
          new HostArrayBufferVar(*web_array_buffer));
      *result = buffer_var->GetPPVar();
    } else if (object_vars_allowed == V8VarConverter::kAllowObjectVars) {
      v8::Local<v8::Object> object = val.As<v8::Object>();
      *result = content::HostGlobals::Get()->
          host_var_tracker()->V8ObjectVarForV8Object(instance, object);
    } else if (val->IsArray()) {
      *result = (new ArrayVar())->GetPPVar();
    } else {
      bool was_resource;
      if (!resource_converter->FromV8Value(val.As<v8::Object>(), context,
                                           result, &was_resource))
        return false;
      if (!was_resource) {
        *result = (new DictionaryVar())->GetPPVar();
      }
    }
  } else {
    // Silently ignore the case where we can't convert to a Var as we may
    // be trying to convert a type that doesn't have a corresponding
    // PP_Var type.
    return true;
  }

  *did_create = true;
  if (val->IsObject() || val->IsString()) {
    visited_handles->insert(
        make_pair(HashedHandle(val->ToObject(context).ToLocalChecked()),
                  ScopedPPVar(ScopedPPVar::PassRef(), *result)));
  }
  return true;
}

bool CanHaveChildren(PP_Var var) {
  return var.type == PP_VARTYPE_ARRAY || var.type == PP_VARTYPE_DICTIONARY;
}

}  // namespace

V8VarConverter::V8VarConverter(PP_Instance instance,
                               AllowObjectVars object_vars_allowed)
    : instance_(instance),
      object_vars_allowed_(object_vars_allowed) {
  resource_converter_.reset(new ResourceConverterImpl(instance));
}

V8VarConverter::V8VarConverter(
    PP_Instance instance,
    std::unique_ptr<ResourceConverter> resource_converter)
    : instance_(instance),
      object_vars_allowed_(kDisallowObjectVars),
      resource_converter_(resource_converter.release()) {}

V8VarConverter::~V8VarConverter() {}

// To/FromV8Value use a stack-based DFS search to traverse V8/Var graph. Each
// iteration, the top node on the stack examined. If the node has not been
// visited yet (i.e. sentinel == false) then it is added to the list of parents
// which contains all of the nodes on the path from the start node to the
// current node. Each of the current nodes children are examined. If they appear
// in the list of parents it means we have a cycle and we return NULL.
// Otherwise, if they can have children, we add them to the stack. If the
// node at the top of the stack has already been visited, then we pop it off the
// stack and erase it from the list of parents.
// static
bool V8VarConverter::ToV8Value(const PP_Var& var,
                               v8::Local<v8::Context> context,
                               v8::Local<v8::Value>* result) {
  v8::Context::Scope context_scope(context);
  v8::Isolate* isolate = context->GetIsolate();
  v8::EscapableHandleScope handle_scope(isolate);

  VarHandleMap visited_ids;
  ParentVarSet parent_ids;

  // The code below needs to reference stack nodes across updates. base::stack
  // is not stable, so we use a circular_deque indexed by integer indices. The
  // back of the deque is the top of the stack.
  base::circular_deque<StackEntry<PP_Var>> stack;
  stack.push_back(StackEntry<PP_Var>(var));
  v8::Local<v8::Value> root;
  bool is_root = true;

  while (!stack.empty()) {
    // This index is stable across updates at the back.
    size_t current_var_index = stack.size() - 1;
    v8::Local<v8::Value> current_v8;

    if (stack.back().sentinel) {
      if (CanHaveChildren(stack[current_var_index].val))
        parent_ids.erase(stack[current_var_index].val.value.as_id);
      stack.pop_back();
      continue;
    } else {
      stack.back().sentinel = true;
    }

    bool did_create = false;
    if (!GetOrCreateV8Value(context, stack[current_var_index].val,
                            object_vars_allowed_, &current_v8, &did_create,
                            &visited_ids, &parent_ids,
                            resource_converter_.get())) {
      return false;
    }

    if (is_root) {
      is_root = false;
      root = current_v8;
    }

    // Add child nodes to the stack.
    if (stack[current_var_index].val.type == PP_VARTYPE_ARRAY) {
      parent_ids.insert(stack[current_var_index].val.value.as_id);
      ArrayVar* array_var = ArrayVar::FromPPVar(stack[current_var_index].val);
      if (!array_var) {
        NOTREACHED();
        return false;
      }
      DCHECK(current_v8->IsArray());
      v8::Local<v8::Array> v8_array = current_v8.As<v8::Array>();

      for (size_t i = 0; i < array_var->elements().size(); ++i) {
        const PP_Var& child_var = array_var->elements()[i].get();
        v8::Local<v8::Value> child_v8;
        if (!GetOrCreateV8Value(context,
                                child_var,
                                object_vars_allowed_,
                                &child_v8,
                                &did_create,
                                &visited_ids,
                                &parent_ids,
                                resource_converter_.get())) {
          return false;
        }
        if (did_create && CanHaveChildren(child_var))
          stack.push_back(child_var);
        if (v8_array->Set(context, static_cast<uint32_t>(i), child_v8)
                .IsNothing()) {
          LOG(ERROR) << "Setter for index " << i << " threw an exception.";
          return false;
        }
      }
    } else if (stack[current_var_index].val.type == PP_VARTYPE_DICTIONARY) {
      parent_ids.insert(stack[current_var_index].val.value.as_id);
      DictionaryVar* dict_var =
          DictionaryVar::FromPPVar(stack[current_var_index].val);
      if (!dict_var) {
        NOTREACHED();
        return false;
      }
      DCHECK(current_v8->IsObject());
      v8::Local<v8::Object> v8_object = current_v8.As<v8::Object>();

      for (auto iter = dict_var->key_value_map().begin();
           iter != dict_var->key_value_map().end(); ++iter) {
        const std::string& key = iter->first;
        const PP_Var& child_var = iter->second.get();
        v8::Local<v8::Value> child_v8;
        if (!GetOrCreateV8Value(context,
                                child_var,
                                object_vars_allowed_,
                                &child_v8,
                                &did_create,
                                &visited_ids,
                                &parent_ids,
                                resource_converter_.get())) {
          return false;
        }
        if (did_create && CanHaveChildren(child_var))
          stack.push_back(child_var);

        if (v8_object
                ->Set(context,
                      v8::String::NewFromUtf8(isolate, key.c_str(),
                                              v8::NewStringType::kInternalized,
                                              key.length())
                          .ToLocalChecked(),
                      child_v8)
                .IsNothing()) {
          LOG(ERROR) << "Setter for property " << key.c_str() << " threw an "
                     << "exception.";
          return false;
        }
      }
    }
  }

  *result = handle_scope.Escape(root);
  return true;
}

V8VarConverter::VarResult V8VarConverter::FromV8Value(
    v8::Local<v8::Value> val,
    v8::Local<v8::Context> context,
    base::OnceCallback<void(const ScopedPPVar&, bool)> callback) {
  VarResult result;
  result.success = FromV8ValueInternal(val, context, &result.var);
  if (!result.success)
    resource_converter_->Reset();
  result.completed_synchronously = !resource_converter_->NeedsFlush();
  if (!result.completed_synchronously)
    resource_converter_->Flush(base::BindOnce(std::move(callback), result.var));

  return result;
}

bool V8VarConverter::FromV8ValueSync(
    v8::Local<v8::Value> val,
    v8::Local<v8::Context> context,
    ppapi::ScopedPPVar* result_var) {
  bool success = FromV8ValueInternal(val, context, result_var);
  if (!success || resource_converter_->NeedsFlush()) {
    resource_converter_->Reset();
    return false;
  }
  return true;
}

bool V8VarConverter::FromV8ValueInternal(
    v8::Local<v8::Value> val,
    v8::Local<v8::Context> context,
    ppapi::ScopedPPVar* result_var) {
  v8::Context::Scope context_scope(context);
  v8::HandleScope handle_scope(context->GetIsolate());

  HandleVarMap visited_handles;
  ParentHandleSet parent_handles;

  base::stack<StackEntry<v8::Local<v8::Value>>> stack;
  stack.push(StackEntry<v8::Local<v8::Value> >(val));
  ScopedPPVar root;
  *result_var = PP_MakeUndefined();
  bool is_root = true;

  while (!stack.empty()) {
    v8::Local<v8::Value> current_v8 = stack.top().val;
    PP_Var current_var;

    if (stack.top().sentinel) {
      stack.pop();
      if (current_v8->IsObject())
        parent_handles.erase(HashedHandle(current_v8.As<v8::Object>()));
      continue;
    } else {
      stack.top().sentinel = true;
    }

    bool did_create = false;
    if (!GetOrCreateVar(current_v8,
                        context,
                        instance_,
                        object_vars_allowed_,
                        &current_var,
                        &did_create,
                        &visited_handles,
                        &parent_handles,
                        resource_converter_.get())) {
      return false;
    }

    if (is_root) {
      is_root = false;
      root = current_var;
    }

    // Add child nodes to the stack.
    if (current_var.type == PP_VARTYPE_ARRAY) {
      DCHECK(current_v8->IsArray());
      v8::Local<v8::Array> v8_array = current_v8.As<v8::Array>();
      parent_handles.insert(HashedHandle(v8_array));

      ArrayVar* array_var = ArrayVar::FromPPVar(current_var);
      if (!array_var) {
        NOTREACHED();
        return false;
      }

      for (uint32_t i = 0; i < v8_array->Length(); ++i) {
        v8::Local<v8::Value> child_v8;
        if (!v8_array->Get(context, i).ToLocal(&child_v8))
          return false;

        if (!v8_array->HasRealIndexedProperty(context, i).FromMaybe(false))
          continue;

        PP_Var child_var;
        if (!GetOrCreateVar(child_v8,
                            context,
                            instance_,
                            object_vars_allowed_,
                            &child_var,
                            &did_create,
                            &visited_handles,
                            &parent_handles,
                            resource_converter_.get())) {
          return false;
        }
        if (did_create && child_v8->IsObject())
          stack.push(child_v8);

        array_var->Set(i, child_var);
      }
    } else if (current_var.type == PP_VARTYPE_DICTIONARY) {
      DCHECK(current_v8->IsObject());
      v8::Local<v8::Object> v8_object = current_v8.As<v8::Object>();
      parent_handles.insert(HashedHandle(v8_object));

      DictionaryVar* dict_var = DictionaryVar::FromPPVar(current_var);
      if (!dict_var) {
        NOTREACHED();
        return false;
      }

      v8::Local<v8::Array> property_names(
          v8_object->GetOwnPropertyNames(context).ToLocalChecked());
      for (uint32_t i = 0; i < property_names->Length(); ++i) {
        v8::Local<v8::Value> key(
            property_names->Get(context, i).ToLocalChecked());

        // Extend this test to cover more types as necessary and if sensible.
        if (!key->IsString() && !key->IsNumber()) {
          NOTREACHED() << "Key \""
                       << *v8::String::Utf8Value(context->GetIsolate(), key)
                       << "\" "
                          "is neither a string nor a number";
          return false;
        }

        v8::Local<v8::String> key_string =
            key->ToString(context).ToLocalChecked();
        // Skip all callbacks: crbug.com/139933
        if (v8_object->HasRealNamedCallbackProperty(context, key_string)
                .ToChecked()) {
          continue;
        }

        v8::String::Utf8Value name_utf8(context->GetIsolate(), key_string);

        v8::Local<v8::Value> child_v8;
        if (!v8_object->Get(context, key).ToLocal(&child_v8))
          return false;

        PP_Var child_var;
        if (!GetOrCreateVar(child_v8,
                            context,
                            instance_,
                            object_vars_allowed_,
                            &child_var,
                            &did_create,
                            &visited_handles,
                            &parent_handles,
                            resource_converter_.get())) {
          return false;
        }
        if (did_create && child_v8->IsObject())
          stack.push(child_v8);

        bool success = dict_var->SetWithStringKey(
            std::string(*name_utf8, name_utf8.length()), child_var);
        DCHECK(success);
      }
    }
  }
  *result_var = root;
  return true;
}

}  // namespace content
