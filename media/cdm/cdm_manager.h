// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_MANAGER_H_
#define MEDIA_CDM_CDM_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "media/base/media_export.h"
#include "media/cdm/cdm_manager_export.h"

namespace media {

class ContentDecryptionModule;

// Provides a singleton registry of CDM instances. This is used to share
// ContentDecryptionModules between the MojoMediaService and
// AndroidVideoDecodeAccelerator, and should be removed along with AVDA in the
// future. (MojoCdmServiceContext serves the same purpose for Media Mojo
// services, but scoped to a single InterfaceFactory.)
class CDM_MANAGER_EXPORT CdmManager {
 public:
  CdmManager();
  ~CdmManager();

  static CdmManager* GetInstance();

  // Returns the CDM associated with |cdm_id|. Can be called on any thread.
  scoped_refptr<ContentDecryptionModule> GetCdm(int cdm_id);

  // Registers the |cdm| for |cdm_id|.
  void RegisterCdm(int cdm_id, scoped_refptr<ContentDecryptionModule> cdm);

  // Unregisters the CDM associated with |cdm_id|.
  void UnregisterCdm(int cdm_id);

 private:
  base::Lock lock_;
  std::map<int, scoped_refptr<ContentDecryptionModule>> cdm_map_
      GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(CdmManager);
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_MANAGER_H_
