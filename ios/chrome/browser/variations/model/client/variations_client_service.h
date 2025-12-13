// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VARIATIONS_MODEL_CLIENT_VARIATIONS_CLIENT_SERVICE_H_
#define IOS_CHROME_BROWSER_VARIATIONS_MODEL_CLIENT_VARIATIONS_CLIENT_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/variations/variations_client.h"

class ProfileIOS;

// Service responsible for variations logic, including providing a
// VariationsClient.
class VariationsClientService : public KeyedService,
                                public variations::VariationsClient {
 public:
  explicit VariationsClientService(ProfileIOS* profile);
  ~VariationsClientService() override;

  // variations::VariationsClient:
  bool IsOffTheRecord() const override;
  variations::mojom::VariationsHeadersPtr GetVariationsHeaders() const override;

 private:
  raw_ptr<ProfileIOS> profile_;
};

#endif  // IOS_CHROME_BROWSER_VARIATIONS_MODEL_CLIENT_VARIATIONS_CLIENT_SERVICE_H_
