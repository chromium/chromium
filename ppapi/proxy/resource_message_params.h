// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_RESOURCE_MESSAGE_PARAMS_H_
#define PPAPI_PROXY_RESOURCE_MESSAGE_PARAMS_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/memory/ref_counted.h"
#include "ipc/ipc_message_utils.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/serialized_handle.h"

namespace base {
class ReadOnlySharedMemoryRegion;
}

namespace ppapi {
namespace proxy {

// Common parameters for resource call and reply params structures below.
class PPAPI_PROXY_EXPORT ResourceMessageParams {
 public:
  virtual ~ResourceMessageParams();

  PP_Resource pp_resource() const { return pp_resource_; }
  int32_t sequence() const { return sequence_; }

  // Note that the caller doesn't take ownership of the returned handles.
  const std::vector<SerializedHandle>& handles() const {
    return handles_->data();
  }

  // Makes ResourceMessageParams leave its handles open, even if they weren't
  // taken using a Take.* function. After this call, no Take.* calls are
  // allowed.
  void ConsumeHandles() const;

  // Returns the handle at the given index if it exists and is of the given
  // type. The corresponding slot in the list is set to an invalid handle.
  // If the index doesn't exist or the handle isn't of the given type, returns
  // an invalid handle.
  // Note that the caller is responsible for closing the returned handle, if it
  // is valid.
  SerializedHandle TakeHandleOfTypeAtIndex(size_t index,
                                           SerializedHandle::Type type) const;

  // Helper functions to return shared memory, socket or file handles passed in
  // the params struct.
  // If the index has a valid handle of the given type, it will be placed in the
  // output parameter, the corresponding slot in the list will be set to an
  // invalid handle, and the function will return true. If the handle doesn't
  // exist or is a different type, the functions will return false and the
  // output parameter will be untouched.
  //
  // Note: 1) the handle could still be a "null" or invalid handle of the right
  //          type and the functions will succeed.
  //       2) the caller is responsible for closing the returned handle, if it
  //          is valid.
  bool TakeReadOnlySharedMemoryRegionAtIndex(
      size_t index,
      base::ReadOnlySharedMemoryRegion* region) const;
  bool TakeUnsafeSharedMemoryRegionAtIndex(
      size_t index,
      base::UnsafeSharedMemoryRegion* region) const;
  bool TakeSocketHandleAtIndex(size_t index,
                               IPC::PlatformFileForTransit* handle) const;
  bool TakeFileHandleAtIndex(size_t index,
                             IPC::PlatformFileForTransit* handle) const;
  void TakeAllHandles(std::vector<SerializedHandle>* handles) const;

  // Appends the given handle to the list of handles sent with the call or
  // reply.
  void AppendHandle(SerializedHandle handle) const;

 protected:
  ResourceMessageParams();
  ResourceMessageParams(PP_Resource resource, int32_t sequence);

  virtual void Serialize(base::Pickle* msg) const;
  virtual bool Deserialize(const base::Pickle* msg, base::PickleIterator* iter);

  // Writes everything except the handles to |msg|.
  void WriteHeader(base::Pickle* msg) const;
  // Writes the handles to |msg|.
  void WriteHandles(base::Pickle* msg) const;
  // Matching deserialize helpers.
  bool ReadHeader(const base::Pickle* msg, base::PickleIterator* iter);
  bool ReadHandles(const base::Pickle* msg, base::PickleIterator* iter);

 private:
  class PPAPI_PROXY_EXPORT SerializedHandles
      : public base::RefCountedThreadSafe<SerializedHandles> {
   public:
    SerializedHandles();
    ~SerializedHandles();

    void set_should_close(bool value) { should_close_ = value; }
    std::vector<SerializedHandle>& data() { return data_; }

   private:
    friend class base::RefCountedThreadSafe<SerializedHandles>;

    // Whether the handles stored in |data_| should be closed when this object
    // goes away.
    //
    // It is set to true by ResourceMessageParams::Deserialize(), so that the
    // receiving side of the params (the host side for
    // ResourceMessageCallParams; the plugin side for
    // ResourceMessageReplyParams) will close those handles which haven't been
    // taken using any of the Take*() methods.
    bool should_close_;
    std::vector<SerializedHandle> data_;
  };

  PP_Resource pp_resource_;

  // Identifier for this message. Sequence numbers are quasi-unique within a
  // resource, but will overlap between different resource objects.
  //
  // If you send a lot of messages, the ID may wrap around. This is OK. All IDs
  // are valid and 0 and -1 aren't special, so those cases won't confuse us.
  // In practice, if you send more than 4 billion messages for a resource, the
  // old ones will be long gone and there will be no collisions.
  //
  // If there is a malicious plugin (or exceptionally bad luck) that causes a
  // wraparound and collision the worst that will happen is that we can get
  // confused between different callbacks. But since these can only cause
  // confusion within the plugin and within callbacks on the same resource,
  // there shouldn't be a security problem.
  int32_t sequence_;

  // A list of all handles transferred in the message. Handles go here so that
  // the NaCl adapter can extract them generally when it rewrites them to
  // go between Windows and NaCl (Posix) apps.
  // TODO(yzshen): Mark it as mutable so that we can take/append handles using a
  // const reference. We need to change all the callers and make it not mutable.
  mutable scoped_refptr<SerializedHandles> handles_;
};

// Parameters common to all ResourceMessage "Call" requests.
class PPAPI_PROXY_EXPORT ResourceMessageCallParams
    : public ResourceMessageParams {
 public:
  ResourceMessageCallParams();
  ResourceMessageCallParams(PP_Resource resource, int32_t sequence);
  ~ResourceMessageCallParams() override;

  void set_has_callback() { has_callback_ = true; }
  bool has_callback() const { return has_callback_; }

  void Serialize(base::Pickle* msg) const override;
  bool Deserialize(const base::Pickle* msg,
                   base::PickleIterator* iter) override;

 private:
  bool has_callback_;
};

// Parameters common to all ResourceMessage "Reply" requests.
class PPAPI_PROXY_EXPORT ResourceMessageReplyParams
    : public ResourceMessageParams {
 public:
  ResourceMessageReplyParams();
  ResourceMessageReplyParams(PP_Resource resource, int32_t sequence);
  ~ResourceMessageReplyParams() override;

  void set_result(int32_t r) { result_ = r; }
  int32_t result() const { return result_; }

  void Serialize(base::Pickle* msg) const override;
  bool Deserialize(const base::Pickle* msg,
                   base::PickleIterator* iter) override;

  // Writes everything except the handles to |msg|.
  void WriteReplyHeader(base::Pickle* msg) const;

 private:
  // Pepper "result code" for the callback.
  int32_t result_;
};

}  // namespace proxy
}  // namespace ppapi

namespace IPC {

template <> struct PPAPI_PROXY_EXPORT
ParamTraits<ppapi::proxy::ResourceMessageCallParams> {
  typedef ppapi::proxy::ResourceMessageCallParams param_type;
  static void Write(base::Pickle* m, const param_type& p) { p.Serialize(m); }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return r->Deserialize(m, iter);
  }
  static void Log(const param_type& p, std::string* l) {
  }
};

template <> struct PPAPI_PROXY_EXPORT
ParamTraits<ppapi::proxy::ResourceMessageReplyParams> {
  typedef ppapi::proxy::ResourceMessageReplyParams param_type;
  static void Write(base::Pickle* m, const param_type& p) { p.Serialize(m); }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return r->Deserialize(m, iter);
  }
  static void Log(const param_type& p, std::string* l) {
  }
};

}  // namespace IPC

#endif  // PPAPI_PROXY_RESOURCE_MESSAGE_PARAMS_H_
