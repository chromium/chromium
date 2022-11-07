// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_CONTEXT_REF_IMPL_H_
#define MEDIA_CDM_CDM_CONTEXT_REF_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/cdm_context.h"
#include "media/base/media_export.h"

namespace media {

class ContentDecryptionModule;

class MEDIA_EXPORT CdmContextRefImpl final : public CdmContextRef {
 public:
  explicit CdmContextRefImpl(scoped_refptr<ContentDecryptionModule> cdm);

  CdmContextRefImpl(const CdmContextRefImpl&) = delete;
  CdmContextRefImpl& operator=(const CdmContextRefImpl&) = delete;

  ~CdmContextRefImpl() final;

  // CdmContextRef implementation.
  CdmContext* GetCdmContext() final;

 private:
  scoped_refptr<ContentDecryptionModule> cdm_;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_CONTEXT_REF_IMPL_H_
