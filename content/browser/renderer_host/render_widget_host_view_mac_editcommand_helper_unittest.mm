// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/render_widget_host_view_mac_editcommand_helper.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>
#include <stdint.h>

#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/visible_time_request_trigger.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_image_transport_factory.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/display/screen.h"

using content::RenderWidgetHostViewMac;

// Bare bones obj-c class for testing purposes.
@interface RenderWidgetHostViewMacEditCommandHelperTestClass : NSObject
@end

@implementation RenderWidgetHostViewMacEditCommandHelperTestClass
@end

// Class that owns a RenderWidgetHostViewMac.
@interface RenderWidgetHostNSViewHostOwner
    : NSObject <RenderWidgetHostNSViewHostOwner> {
  raw_ptr<RenderWidgetHostViewMac> _rwhvm;
}

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)rwhvm;
@end

@implementation RenderWidgetHostNSViewHostOwner

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)rwhvm {
  if ((self = [super init])) {
    _rwhvm = rwhvm;
  }
  return self;
}

- (remote_cocoa::mojom::RenderWidgetHostNSViewHost*)renderWidgetHostNSViewHost {
  return _rwhvm;
}

@end

namespace content {
namespace {

// Returns true if all the edit command names in the array are present in
// test_obj.  edit_commands is a list of NSStrings, selector names are formed
// by appending a trailing ':' to the string.
bool CheckObjectRespondsToEditCommands(NSArray* edit_commands, id test_obj) {
  for (NSString* edit_command_name in edit_commands) {
    NSString* sel_str = [edit_command_name stringByAppendingString:@":"];
    if (![test_obj respondsToSelector:NSSelectorFromString(sel_str)]) {
      return false;
    }
  }
  return true;
}

class RenderWidgetHostDelegateEditCommandCounter
    : public RenderWidgetHostDelegate {
 public:
  RenderWidgetHostDelegateEditCommandCounter() = default;
  ~RenderWidgetHostDelegateEditCommandCounter() override = default;
  unsigned int edit_command_message_count_ = 0;

 private:
  void ExecuteEditCommand(const std::string& command,
                          const std::optional<std::u16string>& value) override {
    edit_command_message_count_++;
  }
  void Undo() override {}
  void Redo() override {}
  void Cut() override {}
  void Copy() override {}
  void Paste() override {}
  void PasteAndMatchStyle() override {}
  void SelectAll() override {}
  VisibleTimeRequestTrigger& GetVisibleTimeRequestTrigger() override {
    return visible_time_request_trigger_;
  }

  VisibleTimeRequestTrigger visible_time_request_trigger_;
};

class RenderWidgetHostViewMacEditCommandHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    ImageTransportFactory::SetFactory(
        std::make_unique<TestImageTransportFactory>());
  }
  void TearDown() override { ImageTransportFactory::Terminate(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

class RenderWidgetHostViewMacEditCommandHelperWithTaskEnvTest
    : public PlatformTest {
 protected:
  void SetUp() override {
    ImageTransportFactory::SetFactory(
        std::make_unique<TestImageTransportFactory>());
  }
  void TearDown() override { ImageTransportFactory::Terminate(); }

 private:
  display::ScopedNativeScreen screen_;
  // This has a MessageLoop for ImageTransportFactory and enables
  // BrowserThread::UI for RecyclableCompositorMac used by
  // RenderWidgetHostViewMac.
  content::BrowserTaskEnvironment task_environment_;
};

}  // namespace

// Tests that editing commands make it through the pipeline all the way to
// RenderWidgetHost.
TEST_F(RenderWidgetHostViewMacEditCommandHelperWithTaskEnvTest,
       TestEditingCommandDelivery) {
  RenderWidgetHostDelegateEditCommandCounter delegate;
  TestBrowserContext browser_context;
  MockRenderProcessHostFactory process_host_factory;
  RenderProcessHost* process_host =
      process_host_factory.CreateRenderProcessHost(&browser_context, nullptr);
  scoped_refptr<SiteInstanceGroup> site_instance_group = base::WrapRefCounted(
      SiteInstanceGroup::CreateForTesting(&browser_context, process_host));
  ui::test::ScopedSetSupportedResourceScaleFactors scoped_supported(
      {ui::k100Percent});

  @autoreleasepool {
    int32_t routing_id = process_host->GetNextRoutingID();
    std::unique_ptr<RenderWidgetHostImpl> render_widget =
        RenderWidgetHostFactory::Create(
            /*frame_tree=*/nullptr, &delegate,
            RenderWidgetHostImpl::DefaultFrameSinkId(*site_instance_group,
                                                     routing_id),
            site_instance_group->GetSafeRef(), routing_id,
            /*hidden=*/false, /*renderer_initiated_creation=*/false);

    ui::WindowResizeHelperMac::Get()->Init(
        base::SingleThreadTaskRunner::GetCurrentDefault());

    // Owned by its |GetInProcessNSView()|, i.e. |rwhv_cocoa|.
    RenderWidgetHostViewMac* rwhv_mac =
        new RenderWidgetHostViewMac(render_widget.get());
    // ARC conversion note: the previous version of this code held this view
    // strongly throughout with a scoped_nsobject. The precise lifetime
    // attribute replicates that but it's not clear if it's necessary.
    [[maybe_unused]] NS_VALID_UNTIL_END_OF_SCOPE RenderWidgetHostViewCocoa*
        rwhv_cocoa = rwhv_mac->GetInProcessNSView();

    NSArray* edit_command_strings = RenderWidgetHostViewMacEditCommandHelper::
        GetEditSelectorNamesForTesting();
    RenderWidgetHostNSViewHostOwner* rwhwvm_owner =
        [[RenderWidgetHostNSViewHostOwner alloc]
            initWithRenderWidgetHostViewMac:rwhv_mac];

    RenderWidgetHostViewMacEditCommandHelper::AddEditingSelectorsToClass(
        [rwhwvm_owner class]);

    for (NSString* edit_command_name in edit_command_strings) {
      NSString* sel_str = [edit_command_name stringByAppendingString:@":"];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
      [rwhwvm_owner performSelector:NSSelectorFromString(sel_str)
                         withObject:nil];
#pragma clang diagnostic pop
    }

    size_t num_edit_commands = edit_command_strings.count;
    EXPECT_EQ(delegate.edit_command_message_count_, num_edit_commands);
    rwhv_cocoa = nil;
  }
  process_host->Cleanup();
  ui::WindowResizeHelperMac::Get()->ShutdownForTests();
}

// Test RenderWidgetHostViewMacEditCommandHelper::AddEditingSelectorsToClass
TEST_F(RenderWidgetHostViewMacEditCommandHelperTest,
       TestAddEditingSelectorsToClass) {
  RenderWidgetHostViewMacEditCommandHelper helper;
  NSArray* edit_command_strings = RenderWidgetHostViewMacEditCommandHelper::
      GetEditSelectorNamesForTesting();
  ASSERT_GT(edit_command_strings.count, 0U);

  // Create a class instance and add methods to the class.
  RenderWidgetHostViewMacEditCommandHelperTestClass* test_obj =
      [[RenderWidgetHostViewMacEditCommandHelperTestClass alloc] init];

  // Check that edit commands aren't already attached to the object.
  ASSERT_FALSE(CheckObjectRespondsToEditCommands(edit_command_strings,
      test_obj));

  RenderWidgetHostViewMacEditCommandHelper::AddEditingSelectorsToClass(
      [test_obj class]);

  // Check that all edit commands where added.
  ASSERT_TRUE(CheckObjectRespondsToEditCommands(edit_command_strings,
      test_obj));

  // AddEditingSelectorsToClass() should be idempotent.
  RenderWidgetHostViewMacEditCommandHelper::AddEditingSelectorsToClass(
      [test_obj class]);

  // Check that all edit commands are still there.
  ASSERT_TRUE(CheckObjectRespondsToEditCommands(edit_command_strings,
      test_obj));
}

// Test RenderWidgetHostViewMacEditCommandHelper::IsMenuItemEnabled.
TEST_F(RenderWidgetHostViewMacEditCommandHelperTest, TestMenuItemEnabling) {
  RenderWidgetHostViewMacEditCommandHelper helper;
  RenderWidgetHostNSViewHostOwner* rwhvm_owner =
      [[RenderWidgetHostNSViewHostOwner alloc] init];

  // The select all menu should always be enabled.
  SEL select_all = NSSelectorFromString(@"selectAll:");
  ASSERT_TRUE(helper.IsMenuItemEnabled(select_all, rwhvm_owner));

  // Random selectors should be enabled by the function.
  SEL garbage_selector = NSSelectorFromString(@"randomGarbageSelector:");
  ASSERT_FALSE(helper.IsMenuItemEnabled(garbage_selector, rwhvm_owner));

  // TODO(jeremy): Currently IsMenuItemEnabled just returns true for all edit
  // selectors.  Once we go past that we should do more extensive testing here.
}

}  // namespace content
