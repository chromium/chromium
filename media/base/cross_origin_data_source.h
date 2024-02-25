// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CROSS_ORIGIN_DATA_SOURCE_H_
#define MEDIA_BASE_CROSS_ORIGIN_DATA_SOURCE_H_

#include "media/base/data_source.h"
#include "media/base/media_export.h"

namespace media {

// This represents any type of DataSource which MAY make cross origin requests,
// as opposed to something like the "MemoryDataSource" which is only reading
// from an already-allocated block of data, and can't be cross origin at all.
class MEDIA_EXPORT CrossOriginDataSource : public DataSource {
 public:
  // https://html.spec.whatwg.org/#cors-cross-origin
  // This must be called after the response arrives.
  virtual bool IsCorsCrossOrigin() const = 0;

  // Returns true if the response includes an Access-Control-Allow-Origin
  // header (that is not "null").
  virtual bool HasAccessControl() const = 0;

  virtual const std::string& GetMimeType() const = 0;

  // Allows the data source to perform any additional initialization steps.
  // This should always be called after constructing, and before other use.
  virtual void Initialize(base::OnceCallback<void(bool)> init_cb) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_CROSS_ORIGIN_DATA_SOURCE_H_
