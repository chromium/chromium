// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_CONSUMER_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_CONSUMER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/webdata_export.h"

// All requests to the web data service are asynchronous. When the request has
// been performed, the data consumer is notified using the following interface.
// As the WebDataServiceConsumer has a WeakPtrFactory, it does not need to
// outlive the WebDataService.
class WEBDATA_EXPORT WebDataServiceConsumer {
 public:
  WebDataServiceConsumer();

  // Called when a request is done. |h| uniquely identifies the request. The
  // |result| can be null if no result is expected or if the database could not
  // be opened.
  virtual void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) = 0;

  base::WeakPtr<WebDataServiceConsumer> GetWebDataServiceConsumerWeakPtr();

 protected:
  virtual ~WebDataServiceConsumer();

 private:
  // Private to prevent name collisions in derived classes.
  base::WeakPtrFactory<WebDataServiceConsumer> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_CONSUMER_H_
