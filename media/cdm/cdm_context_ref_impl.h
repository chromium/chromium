// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_CONTEXT_REF_IMPL_H_
#define MEDIA_CDM_CDM_CONTEXT_REF_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "media/base/cdm_context.h"
#include "media/base/media_export.h"

namespace media {

class ContentDecryptionModule;

class MEDIA_EXPORT CdmContextRefImpl final : public CdmContextRef {
 public:
  explicit CdmContextRefImpl(scoped_refptr<ContentDecryptionModule> cdm);
  ~CdmContextRefImpl() final;

  // CdmContextRef implementation.
  CdmContext* GetCdmContext() final;

 private:
  scoped_refptr<ContentDecryptionModule> cdm_;
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(CdmContextRefImpl);
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_CONTEXT_REF_IMPL_H_
