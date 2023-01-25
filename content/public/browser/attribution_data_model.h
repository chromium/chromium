// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ATTRIBUTION_DATA_MODEL_H_
#define CONTENT_PUBLIC_BROWSER_ATTRIBUTION_DATA_MODEL_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

class CONTENT_EXPORT AttributionDataModel {
 public:
  class CONTENT_EXPORT DataKey {
   public:
    enum class Scope {
      kSource,
      kReport,
    };

    DataKey(url::Origin reporting_origin,
            url::Origin context_origin,
            Scope scope);

    DataKey(const DataKey&);
    DataKey(DataKey&&);

    DataKey& operator=(const DataKey&);
    DataKey& operator=(DataKey&&);

    ~DataKey();

    const url::Origin& reporting_origin() const { return reporting_origin_; }

    const url::Origin& context_origin() const { return context_origin_; }

    Scope scope() const { return scope_; }

    bool operator<(const DataKey&) const;

    bool operator==(const DataKey&) const;

   private:
    url::Origin reporting_origin_;

    url::Origin context_origin_;

    Scope scope_;
  };

  virtual ~AttributionDataModel() = default;

  virtual void GetAllDataKeys(
      base::OnceCallback<void(std::vector<DataKey>)> callback) = 0;

  virtual void RemoveAttributionDataByDataKey(const DataKey& data_key,
                                              base::OnceClosure callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ATTRIBUTION_DATA_MODEL_H_