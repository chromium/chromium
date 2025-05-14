// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/test/task_environment.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/renderer_host.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// Test implementation of mojom::RendererHost.
class TestRendererHostImpl : public mojom::RendererHost {
 public:
  explicit TestRendererHostImpl(
      mojo::PendingReceiver<mojom::RendererHost> receiver)
      : receiver_(this, std::move(receiver)) {}
  TestRendererHostImpl(const TestRendererHostImpl&) = delete;
  TestRendererHostImpl& operator=(const TestRendererHostImpl&) = delete;

  // mojom::RendererHost:
  void AddAPIActionToActivityLog(const std::optional<ExtensionId>& extension_id,
                                 const std::string& call_name,
                                 base::Value::List args,
                                 const std::string& extra) override {}

  void AddEventToActivityLog(const std::optional<ExtensionId>& extension_id,
                             const std::string& call_name,
                             base::Value::List args,
                             const std::string& extra) override {}

  void AddDOMActionToActivityLog(const ExtensionId& extension_id,
                                 const std::string& call_name,
                                 base::Value::List args,
                                 const GURL& url,
                                 const std::u16string& url_title,
                                 int32_t call_type) override {}

  void GetMessageBundle(const ExtensionId& extension_id,
                        GetMessageBundleCallback callback) override {
    std::move(callback).Run({});
  }

 private:
  mojo::Receiver<mojom::RendererHost> receiver_;
};

class RendererHostMojomExtensionIdTest : public testing::Test {
 public:
  RendererHostMojomExtensionIdTest() {
    renderer_host_impl_ = std::make_unique<TestRendererHostImpl>(
        renderer_host_remote_.BindNewPipeAndPassReceiver());
  }

  void AddAPIActionToActivityLog(std::optional<ExtensionId> extension_id) {
    renderer_host_remote_->AddAPIActionToActivityLog(
        extension_id, "test_call_name", base::Value::List(), "test_extra");
    renderer_host_remote_.FlushForTesting();
  }

  void AddEventToActivityLog(std::optional<ExtensionId> extension_id) {
    renderer_host_remote_->AddEventToActivityLog(
        extension_id, "test_call_name", base::Value::List(), "test_extra");
    renderer_host_remote_.FlushForTesting();
  }

  void AddDOMActionToActivityLog(const ExtensionId& extension_id) {
    renderer_host_remote_->AddDOMActionToActivityLog(
        extension_id, "test_call_name", base::Value::List(), GURL(),
        u"test_url_title", 0);
    renderer_host_remote_.FlushForTesting();
  }

  void GetMessageBundle(const ExtensionId& extension_id) {
    renderer_host_remote_->GetMessageBundle(extension_id, base::DoNothing());
    renderer_host_remote_.FlushForTesting();
  }

  bool PipeConnected() { return renderer_host_remote_.is_connected(); }

  void RebindReceiver() {
    renderer_host_impl_.reset();
    renderer_host_remote_.reset();
    renderer_host_impl_ = std::make_unique<TestRendererHostImpl>(
        renderer_host_remote_.BindNewPipeAndPassReceiver());
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::Remote<mojom::RendererHost> renderer_host_remote_;
  std::unique_ptr<TestRendererHostImpl> renderer_host_impl_;
};

// Tests that passing valid extension IDs to mojom::RendererHost implementations
// pass message validation and keep the mojom pipe connected.
TEST_F(RendererHostMojomExtensionIdTest, ValidExtensionId) {
  // Create a valid ExtensionId.
  ExtensionId valid_extension_id(32, 'a');

  AddAPIActionToActivityLog(valid_extension_id);
  ASSERT_TRUE(PipeConnected());

  AddEventToActivityLog(valid_extension_id);
  ASSERT_TRUE(PipeConnected());

  AddDOMActionToActivityLog(valid_extension_id);
  ASSERT_TRUE(PipeConnected());

  GetMessageBundle(valid_extension_id);
  ASSERT_TRUE(PipeConnected());
}

// Tests that passing null extension IDs to mojom::RendererHost implementations
// that accept optional mojom::ExtensionIds will pass message validation and
// keep the mojom pipe connected.
TEST_F(RendererHostMojomExtensionIdTest, NullExtensionId) {
  AddAPIActionToActivityLog(std::nullopt);
  ASSERT_TRUE(PipeConnected());

  AddEventToActivityLog(std::nullopt);
  ASSERT_TRUE(PipeConnected());
}

// Tests that passing invalid extension IDs to mojom::RendererHost
// implementations fail message validation and close the mojom pipe.
TEST_F(RendererHostMojomExtensionIdTest, InvalidExtensionId) {
  // Create an invalid ExtensionId.
  ExtensionId invalid_extension_id = "invalid_id";

  AddAPIActionToActivityLog(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  AddEventToActivityLog(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  AddDOMActionToActivityLog(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  GetMessageBundle(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());
}

// Tests that passing empty extension IDs to mojom::RendererHost
// implementations fail message validation and close the mojom pipe.
TEST_F(RendererHostMojomExtensionIdTest, EmptyExtensionId) {
  ExtensionId empty_extension_id;

  AddAPIActionToActivityLog(empty_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  AddEventToActivityLog(empty_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  AddDOMActionToActivityLog(empty_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  GetMessageBundle(empty_extension_id);
  ASSERT_FALSE(PipeConnected());
}

}  // namespace
}  // namespace extensions
