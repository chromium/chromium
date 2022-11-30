// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_SCOPED_PP_RESOURCE_H_
#define PPAPI_SHARED_IMPL_SCOPED_PP_RESOURCE_H_

#include "ppapi/c/pp_resource.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class Resource;

// This is a version of scoped_refptr but for PP_Resources.
class PPAPI_SHARED_EXPORT ScopedPPResource {
 public:
  struct PassRef {};

  ScopedPPResource();

  // Takes one reference to the given resource.
  explicit ScopedPPResource(PP_Resource resource);

  // Assumes responsibility for one ref that the resource already has.
  explicit ScopedPPResource(const PassRef&, PP_Resource resource);

  // Helper to get the PP_Resource out of the given object and take a reference
  // to it.
  explicit ScopedPPResource(Resource* resource);

  // Implicit copy constructor allowed.
  ScopedPPResource(const ScopedPPResource& other);

  ~ScopedPPResource();

  ScopedPPResource& operator=(PP_Resource resource);
  ScopedPPResource& operator=(const ScopedPPResource& resource);

  // Returns the PP_Resource without affecting the refcounting.
  PP_Resource get() const { return id_; }
  operator PP_Resource() const { return id_; }

  // Returns the PP_Resource, passing the reference to the caller. This class
  // will no longer hold the resource.
  PP_Resource Release();

 private:
  // Helpers to addref or release the id_ if it's non-NULL. The id_ value will
  // be unchanged.
  void CallAddRef();
  void CallRelease();

  PP_Resource id_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_SCOPED_PP_RESOURCE_H_
