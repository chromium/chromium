// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_INITIALIZED_PROMISE_H_
#define MEDIA_BASE_CDM_INITIALIZED_PROMISE_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/cdm_factory.h"
#include "media/base/cdm_promise.h"
#include "media/base/media_export.h"

namespace media {

class ContentDecryptionModule;

// Promise to be resolved when the CDM is initialized. It owns the
// ContentDecryptionModule object until the initialization completes, which it
// then passes to |cdm_created_cb|.
class MEDIA_EXPORT CdmInitializedPromise : public SimpleCdmPromise {
 public:
  CdmInitializedPromise(CdmCreatedCB cdm_created_cb,
                        scoped_refptr<ContentDecryptionModule> cdm);
  ~CdmInitializedPromise() override;

  // SimpleCdmPromise implementation.
  void resolve() override;
  void reject(CdmPromise::Exception exception_code,
              uint32_t system_code,
              const std::string& error_message) override;

 private:
  CdmCreatedCB cdm_created_cb_;

  // Holds a ref-count of the CDM.
  scoped_refptr<ContentDecryptionModule> cdm_;
};

}  // namespace media

#endif  // MEDIA_BASE_CDM_INITIALIZED_PROMISE_H_
