// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_DOM_STORAGE_TEST_STORAGE_AREA_TEST_UTIL_H_
#define CONTENT_BROWSER_DOM_STORAGE_TEST_STORAGE_AREA_TEST_UTIL_H_

#include <stdint.h>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"

// Utility functions and classes for testing StorageArea implementations.

namespace content {
namespace test {

// Creates a callback that sets the given |success_out| to the boolean argument,
// and then calls |callback|.
base::OnceCallback<void(bool)> MakeSuccessCallback(base::OnceClosure callback,
                                                   bool* success_out);

// Does a |Put| call on the given |area| and waits until the response is
// received. Returns if the call was successful.
bool PutSync(blink::mojom::StorageArea* area,
             const std::vector<uint8_t>& key,
             const std::vector<uint8_t>& value,
             const base::Optional<std::vector<uint8_t>>& old_value,
             const std::string& source);

bool GetSync(blink::mojom::StorageArea* area,
             const std::vector<uint8_t>& key,
             std::vector<uint8_t>* data_out);

bool GetAllSync(blink::mojom::StorageArea* area,
                std::vector<blink::mojom::KeyValuePtr>* data_out);

// Unlike GetAllSync above, this method uses
// mojo::MakeRequestAssociatedWithDedicatedPipe for the GetAllCallback object's
// binding to the area. This can be necessary if the area is an
// implementation and not a binding with it's own pipe already.
bool GetAllSyncOnDedicatedPipe(
    blink::mojom::StorageArea* area,
    std::vector<blink::mojom::KeyValuePtr>* data_out);

// Does a |Delete| call on the area and waits until the response is
// received. Returns if the call was successful.
bool DeleteSync(blink::mojom::StorageArea* area,
                const std::vector<uint8_t>& key,
                const base::Optional<std::vector<uint8_t>>& client_old_value,
                const std::string& source);

// Does a |DeleteAll| call on the area and waits until the response is
// received. Returns if the call was successful.
bool DeleteAllSync(blink::mojom::StorageArea* area, const std::string& source);

// Creates a callback that simply sets the  |*_out| variables to the arguments.
base::OnceCallback<void(bool, std::vector<blink::mojom::KeyValuePtr>)>
MakeGetAllCallback(bool* success_out,
                   std::vector<blink::mojom::KeyValuePtr>* data_out);

// Utility class to help using the StorageArea::GetAll method. Use
// |CreateAndBind| to create the PtrInfo to send to the |GetAll| method.
// When the call is complete, the |callback| will be called.
class GetAllCallback : public blink::mojom::StorageAreaGetAllCallback {
 public:
  static mojo::PendingAssociatedRemote<blink::mojom::StorageAreaGetAllCallback>
  CreateAndBind(bool* result, base::OnceClosure callback);

  static mojo::PendingAssociatedRemote<blink::mojom::StorageAreaGetAllCallback>
  CreateAndBindOnDedicatedPipe(bool* result, base::OnceClosure callback);

  ~GetAllCallback() override;

 private:
  GetAllCallback(bool* result, base::OnceClosure callback);

  void Complete(bool success) override;

  bool* result_;
  base::OnceClosure callback_;
};

// Mock observer implementation for use with StorageArea.
class MockLevelDBObserver : public blink::mojom::StorageAreaObserver {
 public:
  MockLevelDBObserver();
  ~MockLevelDBObserver() override;

  MOCK_METHOD3(KeyAdded,
               void(const std::vector<uint8_t>& key,
                    const std::vector<uint8_t>& value,
                    const std::string& source));
  MOCK_METHOD4(KeyChanged,
               void(const std::vector<uint8_t>& key,
                    const std::vector<uint8_t>& new_value,
                    const std::vector<uint8_t>& old_value,
                    const std::string& source));
  MOCK_METHOD3(KeyDeleted,
               void(const std::vector<uint8_t>& key,
                    const std::vector<uint8_t>& old_value,
                    const std::string& source));
  MOCK_METHOD1(AllDeleted, void(const std::string& source));
  MOCK_METHOD1(ShouldSendOldValueOnMutations, void(bool value));

  mojo::PendingAssociatedRemote<blink::mojom::StorageAreaObserver> Bind();

 private:
  mojo::AssociatedReceiver<blink::mojom::StorageAreaObserver> receiver_{this};
};

}  // namespace test
}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_TEST_STORAGE_AREA_TEST_UTIL_H_
