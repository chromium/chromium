// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_FACTORY_H_

#include <gmock/gmock.h>
#include <memory>

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_factory.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class WebIDBCallbacks;
class WebIDBDatabaseCallbacks;
class WebSecurityOrigin;
class WebString;

class MockWebIDBFactory : public testing::StrictMock<blink::WebIDBFactory> {
 public:
  ~MockWebIDBFactory() override;

  static std::unique_ptr<MockWebIDBFactory> Create();

  MOCK_METHOD3(GetDatabaseInfo,
               void(WebIDBCallbacks*,
                    const WebSecurityOrigin&,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD3(GetDatabaseNames,
               void(WebIDBCallbacks*,
                    const WebSecurityOrigin&,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD7(Open,
               void(const WebString& name,
                    long long version,
                    long long transaction_id,
                    WebIDBCallbacks*,
                    WebIDBDatabaseCallbacks*,
                    const WebSecurityOrigin&,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD5(DeleteDatabase,
               void(const WebString& name,
                    WebIDBCallbacks*,
                    const WebSecurityOrigin&,
                    bool force_close,
                    scoped_refptr<base::SingleThreadTaskRunner>));

 private:
  MockWebIDBFactory();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_FACTORY_H_
