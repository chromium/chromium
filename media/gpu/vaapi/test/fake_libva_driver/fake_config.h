// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_CONFIG_H_
#define MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_CONFIG_H_

#include <va/va.h>

#include <vector>

namespace media::internal {

// Class used for tracking a VAConfig and all information relevant to it.
// All objects of this class are immutable and thread safe.
class FakeConfig {
 public:
  using IdType = VAConfigID;

  FakeConfig(IdType id,
             VAProfile profile,
             VAEntrypoint entrypoint,
             std::vector<VAConfigAttrib> attrib_list);
  FakeConfig(const FakeConfig&) = delete;
  FakeConfig& operator=(const FakeConfig&) = delete;
  ~FakeConfig();

  IdType GetID() const;
  VAProfile GetProfile() const;
  VAEntrypoint GetEntrypoint() const;
  const std::vector<VAConfigAttrib>& GetConfigAttribs() const;

 private:
  const IdType id_;
  const VAProfile profile_;
  const VAEntrypoint entrypoint_;
  const std::vector<VAConfigAttrib> attrib_list_;
};

}  // namespace media::internal

#endif  // MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_CONFIG_H_