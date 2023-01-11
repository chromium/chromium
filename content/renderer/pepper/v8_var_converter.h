// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_V8_VAR_CONVERTER_H_
#define CONTENT_RENDERER_PEPPER_V8_VAR_CONVERTER_H_

#include <memory>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "v8/include/v8-forward.h"

namespace content {

class ResourceConverter;

class CONTENT_EXPORT V8VarConverter {
 public:
  // Whether or not to allow converting object vars. If they are not allowed
  // and they are passed in, conversion will fail.
  enum AllowObjectVars {
    kDisallowObjectVars,
    kAllowObjectVars
  };
  V8VarConverter(PP_Instance instance, AllowObjectVars object_vars_allowed);

  // Constructor for testing.
  V8VarConverter(PP_Instance instance,
                 std::unique_ptr<ResourceConverter> resource_converter);

  V8VarConverter(const V8VarConverter&) = delete;
  V8VarConverter& operator=(const V8VarConverter&) = delete;

  ~V8VarConverter();

  // Converts the given PP_Var to a v8::Value. True is returned upon success.
  bool ToV8Value(const PP_Var& var,
                 v8::Local<v8::Context> context,
                 v8::Local<v8::Value>* result);

  struct VarResult {
   public:
    VarResult() : completed_synchronously(false), success(false) {}

    // True if the conversion completed synchronously and the callback will not
    // be called.
    bool completed_synchronously;

    // True if the conversion was successful. Only valid if
    // |completed_synchronously| is true.
    bool success;

    // The result if the conversion was successful. Only valid if
    // |completed_synchronously| and |success| are true.
    ppapi::ScopedPPVar var;
  };

  // Converts the given v8::Value to a PP_Var. Every PP_Var in the reference
  // graph in the result will have a refcount equal to the number of references
  // to it in the graph. The root of the result will have one additional
  // reference. The callback is run when conversion is complete with the
  // resulting var and a bool indicating success or failure. Conversion may be
  // asynchronous because converting some resources may result in communication
  // across IPC. |context| is guaranteed to only be used synchronously. If
  // the conversion can occur synchronously, |callback| will not be run,
  // otherwise it will be run.
  VarResult FromV8Value(
      v8::Local<v8::Value> val,
      v8::Local<v8::Context> context,
      base::OnceCallback<void(const ppapi::ScopedPPVar&, bool)> callback);
  bool FromV8ValueSync(v8::Local<v8::Value> val,
                       v8::Local<v8::Context> context,
                       ppapi::ScopedPPVar* result_var);
 private:
  // Returns true on success, false on failure.
  bool FromV8ValueInternal(v8::Local<v8::Value> val,
                           v8::Local<v8::Context> context,
                           ppapi::ScopedPPVar* result_var);

  PP_Instance instance_;

  // Whether or not to support conversion to PP_VARTYPE_OBJECT.
  AllowObjectVars object_vars_allowed_;

  // The converter to use for converting V8 vars to resources.
  std::unique_ptr<ResourceConverter> resource_converter_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_V8_VAR_CONVERTER_H_
