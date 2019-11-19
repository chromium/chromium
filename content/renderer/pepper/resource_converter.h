// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_RESOURCE_CONVERTER_H_
#define CONTENT_RENDERER_PEPPER_RESOURCE_CONVERTER_H_

#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/renderer/pepper/host_resource_var.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"
#include "v8/include/v8.h"

namespace IPC {
class Message;
}

namespace content {

// This class is responsible for converting V8 vars to Pepper resources.
class CONTENT_EXPORT ResourceConverter {
 public:
  virtual ~ResourceConverter();

  // Reset the state of the resource converter.
  virtual void Reset() = 0;

  // Returns true if Flush() needs to be called before using any vars created
  // by the resource converter.
  virtual bool NeedsFlush() = 0;

  // If NeedsFlush() is true then Flush() must be called before any vars created
  // by the ResourceConverter are valid. It handles creating any resource hosts
  // that need to be created. |callback| will always be called asynchronously.
  virtual void Flush(base::OnceCallback<void(bool)> callback) = 0;

  // Attempts to convert a V8 object to a PP_Var with type PP_VARTYPE_RESOURCE.
  // On success, writes the resulting var to |result|, sets |was_resource| to
  // true and returns true. If |val| is not a resource, sets |was_resource| to
  // false and returns true. If an error occurs, returns false.
  virtual bool FromV8Value(v8::Local<v8::Object> val,
                           v8::Local<v8::Context> context,
                           PP_Var* result,
                           bool* was_resource) = 0;

  // Attempts to convert a PP_Var to a V8 object. |var| must have type
  // PP_VARTYPE_RESOURCE. On success, writes the resulting value to |result| and
  // returns true. If an error occurs, returns false.
  virtual bool ToV8Value(const PP_Var& var,
                         v8::Local<v8::Context> context,
                         v8::Local<v8::Value>* result) = 0;
};

class ResourceConverterImpl : public ResourceConverter {
 public:
  explicit ResourceConverterImpl(PP_Instance instance);
  ~ResourceConverterImpl() override;

  // ResourceConverter overrides.
  void Reset() override;
  bool NeedsFlush() override;
  void Flush(base::OnceCallback<void(bool)> callback) override;
  bool FromV8Value(v8::Local<v8::Object> val,
                   v8::Local<v8::Context> context,
                   PP_Var* result,
                   bool* was_resource) override;
  bool ToV8Value(const PP_Var& var,
                 v8::Local<v8::Context> context,
                 v8::Local<v8::Value>* result) override;

 private:
  // Creates a resource var with the given |pending_renderer_id| and
  // |create_message| to be sent to the plugin.
  scoped_refptr<HostResourceVar> CreateResourceVar(
      int pending_renderer_id,
      const IPC::Message& create_message);
  // Creates a resource var with the given |pending_renderer_id| and
  // |create_message| to be sent to the plugin. Also sends
  // |browser_host_create_message| to the browser, and asynchronously stores the
  // resulting browser host ID in the newly created var.
  scoped_refptr<HostResourceVar> CreateResourceVarWithBrowserHost(
      int pending_renderer_id,
      const IPC::Message& create_message,
      const IPC::Message& browser_host_create_message);

  // The instance this ResourceConverter is associated with.
  PP_Instance instance_;

  // A list of the messages to create the browser hosts. This is a parallel
  // array to |browser_vars|. It is kept as a parallel array so that it can be
  // conveniently passed to |CreateBrowserResourceHosts|.
  std::vector<IPC::Message> browser_host_create_messages_;
  // A list of the resource vars associated with browser hosts.
  std::vector<scoped_refptr<HostResourceVar> > browser_vars_;

  DISALLOW_COPY_AND_ASSIGN(ResourceConverterImpl);
};

}  // namespace content
#endif  // CONTENT_RENDERER_PEPPER_RESOURCE_CONVERTER_H_
