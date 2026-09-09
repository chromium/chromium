// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <optional>
#import <tuple>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/json/json_reader.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "url/gurl.h"

namespace {

using optimization_guide::proto::Coordinate;

// LINT.IfChange(DragAndReleaseToolResultCode)
// TODO(crbug.com/472287126): Move this into DragAndReleaseJavaScriptFeature.
enum class DragAndReleaseToolResultCode {
  kOk = 0,
  kFromCoordinatesOutOfBounds = 1,
  kToCoordinatesOutOfBounds = 2,
  kFromInvalidDomNodeId = 3,
  kToInvalidDomNodeId = 4,
  kDragSuppressed = 5,
  kFromElementDisabled = 6,
  kToElementDisabled = 7,
};
// LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/resources/drag_and_release_tool.ts:DragAndReleaseToolResultCode)

enum class CallType {
  kByCoordinate,
  kByNodeId,
};

// Matches a `base::Value` dictionary for a TouchEvent with the expected type,
// stable touch identifier, and `cancelable: true`.
MATCHER_P2(HasTouchEvent, expected_type, expected_touch_id, "") {
  if (!arg.is_dict()) {
    return false;
  }
  const base::DictValue& dict = arg.GetDict();
  const std::string* type = dict.FindString("type");
  std::optional<int> touch_id = dict.FindInt("touchIdentifier");
  std::optional<bool> cancelable = dict.FindBool("cancelable");
  return type && *type == expected_type && touch_id &&
         *touch_id == expected_touch_id && cancelable && *cancelable;
}

// Matches a `base::Value` dictionary for a DragEvent verifying type,
// `bubbles: true`, and `cancelable: true`.
MATCHER_P(HasDragEvent, expected_type, "") {
  if (!arg.is_dict()) {
    return false;
  }
  const base::DictValue& dict = arg.GetDict();
  const std::string* type = dict.FindString("type");
  std::optional<bool> bubbles = dict.FindBool("bubbles");
  std::optional<bool> cancelable = dict.FindBool("cancelable");
  return type && *type == expected_type && bubbles && *bubbles && cancelable &&
         *cancelable;
}

// Matches a `base::Value` dictionary for a MouseEvent verifying type,
// `bubbles: true`, and `cancelable: true`.
MATCHER_P(HasMouseEvent, expected_type, "") {
  if (!arg.is_dict()) {
    return false;
  }
  const base::DictValue& dict = arg.GetDict();
  const std::string* type = dict.FindString("type");
  std::optional<bool> bubbles = dict.FindBool("bubbles");
  std::optional<bool> cancelable = dict.FindBool("cancelable");
  return type && *type == expected_type && bubbles && *bubbles && cancelable &&
         *cancelable;
}

// Returns whether the center of the element with `element_id` is inside the
// viewport of `web_view`.
bool IsElementCenterInViewport(WKWebView* web_view,
                               const std::string& element_id) {
  const std::string script = base::StringPrintf(
      R"(
      (function() {
        const el = document.getElementById('%s');
        if (!el) return false;
        const rect = el.getBoundingClientRect();
        const client_x = rect.left + rect.width / 2.0;
        const client_y = rect.top + rect.height / 2.0;
        return client_x >= 0.0 && client_x <= window.innerWidth &&
               client_y >= 100.0 && client_y <= window.innerHeight - 100.0;
      })();
    )",
      element_id.c_str());
  id result =
      web::test::ExecuteJavaScript(web_view, base::SysUTF8ToNSString(script));
  return [base::apple::ObjCCast<NSNumber>(result) boolValue];
}

class DragAndReleaseToolJavaScriptTest : public web::JavascriptTest {
 public:
  DragAndReleaseToolJavaScriptTest() {
    scoped_feature_list_.InitAndEnableFeature(
        web::features::kAssertOnJavaScriptErrors);
  }

 protected:
  void SetUp() override {
    web::JavascriptTest::SetUp();
    web_view().frame = CGRectMake(0.0, 0.0, 1000.0, 1000.0);

    test_server_.ServeFilesFromSourceDirectory(
        base::FilePath("components/test/data/"));
    ASSERT_TRUE(test_server_.Start());

    AddGCrWebScript();
    AddUserScript(@"dom_node_ids_test");
    AddUserScript(@"drag_and_release_tool");

    ASSERT_TRUE(LoadUrl(test_server_.GetURL("/actor/drag.html")));

    (void)web::test::ExecuteJavaScript(web_view(), @R"(
          (function() {
            document.getElementById('dragLogger')
                .setAttribute('draggable', 'true');
            document.getElementById('pointerLogger')
                .setAttribute('draggable', 'true');
            window.dispatched_events = [];
            const events = [
              'touchstart', 'touchmove', 'touchend',
              'mousedown', 'mousemove', 'mouseup',
              'dragstart', 'drag', 'dragenter', 'dragover',
              'dragleave', 'drop', 'dragend'
            ];
            const el = document.getElementById('dragLogger');
            const pl = document.getElementById('pointerLogger');
            const logHandler = (e) => {
              const item = {
                'type': e.type,
                'bubbles': Boolean(e.bubbles),
                'cancelable': Boolean(e.cancelable),
                'clientX': typeof e.clientX === 'number' ? e.clientX : 0,
                'clientY': typeof e.clientY === 'number' ? e.clientY : 0,
              };
              if (e.touches && e.touches.length > 0) {
                item['touchIdentifier'] = e.touches[0].identifier;
                item['touchClientX'] = e.touches[0].clientX;
                item['touchClientY'] = e.touches[0].clientY;
              } else if (e.changedTouches && e.changedTouches.length > 0) {
                item['touchIdentifier'] = e.changedTouches[0].identifier;
                item['touchClientX'] = e.changedTouches[0].clientX;
                item['touchClientY'] = e.changedTouches[0].clientY;
              }
              if (typeof e.buttons === 'number') {
                item['buttons'] = e.buttons;
              }
              window.dispatched_events.push(item);
            };
            for (const ev of events) {
              el.addEventListener(ev, logHandler);
              pl.addEventListener(ev, logHandler);
            }
          })();
        )");
  }

  // Executes dragAndRelease with serialized target dictionaries.
  NSDictionary* DragAndRelease(NSDictionary* from_target,
                               NSDictionary* to_target) {
    NSError* from_error = nil;
    NSData* from_data = [NSJSONSerialization dataWithJSONObject:from_target
                                                        options:0
                                                          error:&from_error];
    NSError* to_error = nil;
    NSData* to_data = [NSJSONSerialization dataWithJSONObject:to_target
                                                      options:0
                                                        error:&to_error];
    EXPECT_NE(from_data, nil);
    EXPECT_NE(to_data, nil);
    if (!from_data || !to_data) {
      return nil;
    }

    NSString* from_json = [[NSString alloc] initWithData:from_data
                                                encoding:NSUTF8StringEncoding];
    NSString* to_json = [[NSString alloc] initWithData:to_data
                                              encoding:NSUTF8StringEncoding];

    const std::string script =
        base::StringPrintf(R"(
        __gCrWeb.getRegisteredApi('drag_and_release_tool')
                .getFunction('dragAndRelease')(%s, %s)
        )",
                           base::SysNSStringToUTF8(from_json).c_str(),
                           base::SysNSStringToUTF8(to_json).c_str());
    id result = web::test::ExecuteJavaScript(web_view(),
                                             base::SysUTF8ToNSString(script));
    return base::apple::ObjCCast<NSDictionary>(result);
  }

  // Executes dragAndRelease using viewport coordinates.
  NSDictionary* DragAndReleaseByCoordinates(double from_x,
                                            double from_y,
                                            int from_pixel_type,
                                            double to_x,
                                            double to_y,
                                            int to_pixel_type) {
    NSDictionary* from_target = @{
      @"coordinate" : @{
        @"x" : @(from_x),
        @"y" : @(from_y),
        @"pixelType" : @(from_pixel_type)
      }
    };
    NSDictionary* to_target = @{
      @"coordinate" :
          @{@"x" : @(to_x), @"y" : @(to_y), @"pixelType" : @(to_pixel_type)}
    };
    return DragAndRelease(from_target, to_target);
  }

  // Executes dragAndRelease using DOM node IDs.
  NSDictionary* DragAndReleaseByNodeIds(int from_node_id, int to_node_id) {
    return DragAndRelease(
        @{@"contentNodeId" : @(from_node_id)},
        @{@"contentNodeId" : @(to_node_id)});
  }

  // Returns the comma-separated event log recorded on #dragLogger.
  std::string GetEventLogString() {
    id result =
        web::test::ExecuteJavaScript(web_view(), @"event_log.join(',')");
    NSString* log_str = base::apple::ObjCCast<NSString>(result);
    return log_str ? base::SysNSStringToUTF8(log_str) : "";
  }

  // Returns the dispatched events list recorded as JS objects on the test page.
  std::optional<base::ListValue> GetDispatchedEvents() {
    id result = web::test::ExecuteJavaScript(
        web_view(), @"JSON.stringify(window.dispatched_events)");
    NSString* json_str = base::apple::ObjCCast<NSString>(result);
    if (!json_str) {
      return std::nullopt;
    }
    return base::JSONReader::ReadList(base::SysNSStringToUTF8(json_str), 0);
  }

  // Retrieves the client bounding rect of the element with the specified ID.
  NSDictionary* GetElementClientRect(const std::string& element_id) {
    const std::string script = base::StringPrintf(
        R"(
        (function() {
          const el = document.getElementById('%s');
          if (!el) return null;
          const rect = el.getBoundingClientRect();
          return {
            left: rect.left,
            top: rect.top,
            width: rect.width,
            height: rect.height
          };
        })();
      )",
        element_id.c_str());
    id result = web::test::ExecuteJavaScript(web_view(),
                                             base::SysUTF8ToNSString(script));
    return base::apple::ObjCCast<NSDictionary>(result);
  }

  // Retrieves or creates the DOM node ID for the element with the specified ID.
  int GetNodeId(const std::string& element_id) {
    std::string script = base::StringPrintf(
        R"(
        (function() {
          const el = document.getElementById('%s');
          if (!el) return -1;
          return __gCrWeb.getRegisteredApi('dom_node_ids_test')
                  .getFunction('getOrCreateNodeId')(el);
        })();
      )",
        element_id.c_str());
    id node_id_result = web::test::ExecuteJavaScript(
        web_view(), base::SysUTF8ToNSString(script));
    NSNumber* number_result = base::apple::ObjCCast<NSNumber>(node_id_result);
    return number_result ? [number_result intValue] : -1;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer test_server_;
};

// Tests that dragging from or to out-of-bounds coordinates fails appropriately.
TEST_F(DragAndReleaseToolJavaScriptTest, CoordinatesOutOfBounds) {
  // Source out of bounds.
  NSDictionary* from_oob = DragAndReleaseByCoordinates(
      -100.0, -100.0, static_cast<int>(Coordinate::PIXEL_TYPE_DIPS), 150.0,
      150.0, static_cast<int>(Coordinate::PIXEL_TYPE_DIPS));
  ASSERT_NE(from_oob, nil);
  ASSERT_NE(from_oob[@"resultCode"], nil);
  EXPECT_NSEQ(from_oob[@"resultCode"],
              @(static_cast<int>(
                  DragAndReleaseToolResultCode::kFromCoordinatesOutOfBounds)));

  // Destination out of bounds.
  NSDictionary* to_oob = DragAndReleaseByCoordinates(
      150.0, 150.0, static_cast<int>(Coordinate::PIXEL_TYPE_DIPS), -100.0,
      -100.0, static_cast<int>(Coordinate::PIXEL_TYPE_DIPS));
  ASSERT_NE(to_oob, nil);
  ASSERT_NE(to_oob[@"resultCode"], nil);
  EXPECT_NSEQ(to_oob[@"resultCode"],
              @(static_cast<int>(
                  DragAndReleaseToolResultCode::kToCoordinatesOutOfBounds)));
}

// Tests that dragging from or to invalid DOM node IDs fails appropriately.
TEST_F(DragAndReleaseToolJavaScriptTest, InvalidDomNodeIds) {
  int valid_node_id = GetNodeId("dragLogger");
  ASSERT_GT(valid_node_id, 0);

  // Invalid source node ID.
  NSDictionary* from_invalid =
      DragAndReleaseByNodeIds(/*from_node_id=*/999999, valid_node_id);
  ASSERT_NE(from_invalid, nil);
  ASSERT_NE(from_invalid[@"resultCode"], nil);
  EXPECT_NSEQ(
      from_invalid[@"resultCode"],
      @(static_cast<int>(DragAndReleaseToolResultCode::kFromInvalidDomNodeId)));

  // Invalid destination node ID.
  NSDictionary* to_invalid =
      DragAndReleaseByNodeIds(valid_node_id, /*to_node_id=*/999999);
  ASSERT_NE(to_invalid, nil);
  ASSERT_NE(to_invalid[@"resultCode"], nil);
  EXPECT_NSEQ(
      to_invalid[@"resultCode"],
      @(static_cast<int>(DragAndReleaseToolResultCode::kToInvalidDomNodeId)));
}

// Tests that dragging from or to a disabled element fails appropriately.
TEST_F(DragAndReleaseToolJavaScriptTest, DisabledElements) {
  int pointer_id = GetNodeId("pointerLogger");
  int drag_id = GetNodeId("dragLogger");
  ASSERT_GT(pointer_id, 0);
  ASSERT_GT(drag_id, 0);

  // Source element disabled.
  (void)web::test::ExecuteJavaScript(web_view(), @R"(
        document.getElementById('pointerLogger').setAttribute('disabled', '');
      )");
  NSDictionary* from_disabled = DragAndReleaseByNodeIds(pointer_id, drag_id);
  ASSERT_NE(from_disabled, nil);
  ASSERT_NE(from_disabled[@"resultCode"], nil);
  EXPECT_NSEQ(
      from_disabled[@"resultCode"],
      @(static_cast<int>(DragAndReleaseToolResultCode::kFromElementDisabled)));

  // Reset source and disable destination element.
  (void)web::test::ExecuteJavaScript(web_view(), @R"(
        document.getElementById('pointerLogger').removeAttribute('disabled');
        document.getElementById('dragLogger').setAttribute('disabled', '');
      )");
  NSDictionary* to_disabled = DragAndReleaseByNodeIds(pointer_id, drag_id);
  ASSERT_NE(to_disabled, nil);
  ASSERT_NE(to_disabled[@"resultCode"], nil);
  EXPECT_NSEQ(
      to_disabled[@"resultCode"],
      @(static_cast<int>(DragAndReleaseToolResultCode::kToElementDisabled)));
}

// Tests that dragging a form element whose properties are clobbered by named
// child inputs does not crash due to DOM clobbering.
TEST_F(DragAndReleaseToolJavaScriptTest, DomClobbering_Protected) {
  (void)web::test::ExecuteJavaScript(web_view(), @R"(
        const form = document.createElement('form');
        form.id = 'clobberedForm';
        form.style.width = '100px';
        form.style.height = '100px';

        const inputAttr = document.createElement('input');
        inputAttr.name = 'hasAttribute';
        form.appendChild(inputAttr);

        const inputType = document.createElement('input');
        inputType.name = 'nodeType';
        form.appendChild(inputType);

        const inputParent = document.createElement('input');
        inputParent.name = 'parentElement';
        form.appendChild(inputParent);

        document.body.appendChild(form);
        true;
      )");
  int form_id = GetNodeId("clobberedForm");
  int drag_id = GetNodeId("dragLogger");
  ASSERT_GT(form_id, 0);
  ASSERT_GT(drag_id, 0);

  NSDictionary* result = DragAndReleaseByNodeIds(form_id, drag_id);
  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_NSEQ(result[@"resultCode"],
              @(static_cast<int>(DragAndReleaseToolResultCode::kOk)));
}

// Test that dragging a non-draggable element dispatches touch events rather
// than HTML5 drag or mouse events.
TEST_F(DragAndReleaseToolJavaScriptTest,
       NonDraggableElement_DispatchesTouchOnly) {
  // Remove draggable attribute from pointerLogger and dragLogger.
  (void)web::test::ExecuteJavaScript(web_view(), @R"(
        document.getElementById('pointerLogger').removeAttribute('draggable');
        document.getElementById('dragLogger').removeAttribute('draggable');
        window.dispatched_events = [];
      )");

  int from_node_id = GetNodeId("pointerLogger");
  int to_node_id = GetNodeId("dragLogger");
  ASSERT_GT(from_node_id, 0);
  ASSERT_GT(to_node_id, 0);

  NSDictionary* result = DragAndReleaseByNodeIds(from_node_id, to_node_id);
  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_NSEQ(result[@"resultCode"],
              @(static_cast<int>(DragAndReleaseToolResultCode::kOk)));

  std::optional<base::ListValue> events = GetDispatchedEvents();
  ASSERT_TRUE(events.has_value());
  EXPECT_FALSE(events->empty());

  // Verify touch events are dispatched for direct manipulation.
  constexpr int kExpectedTouchId = 0;
  EXPECT_THAT(*events,
              testing::Contains(HasTouchEvent("touchstart", kExpectedTouchId)));
  EXPECT_THAT(*events,
              testing::Contains(HasTouchEvent("touchmove", kExpectedTouchId)));
  EXPECT_THAT(*events,
              testing::Contains(HasTouchEvent("touchend", kExpectedTouchId)));

  // Verify mouse events and HTML5 drag events are NOT dispatched.
  EXPECT_THAT(*events,
              testing::Not(testing::Contains(HasMouseEvent("mousedown"))));
  EXPECT_THAT(*events,
              testing::Not(testing::Contains(HasDragEvent("dragstart"))));
  EXPECT_THAT(*events, testing::Not(testing::Contains(HasDragEvent("drop"))));
  EXPECT_THAT(*events,
              testing::Not(testing::Contains(HasDragEvent("dragend"))));
}

// Tests dragging by coordinates using physical pixels with devicePixelRatio.
TEST_F(DragAndReleaseToolJavaScriptTest,
       DragByCoordinate_SupportsPhysicalPixels) {
  NSDictionary* from_rect = GetElementClientRect("pointerLogger");
  ASSERT_NE(from_rect, nil);
  NSDictionary* to_rect = GetElementClientRect("dragLogger");
  ASSERT_NE(to_rect, nil);

  double from_x = [from_rect[@"left"] doubleValue] +
                  [from_rect[@"width"] doubleValue] / 2.0;
  double from_y = [from_rect[@"top"] doubleValue] +
                  [from_rect[@"height"] doubleValue] / 2.0;
  double to_x =
      [to_rect[@"left"] doubleValue] + [to_rect[@"width"] doubleValue] / 2.0;
  double to_y =
      [to_rect[@"top"] doubleValue] + [to_rect[@"height"] doubleValue] / 2.0;

  constexpr double kDpr = 2.0;
  (void)web::test::ExecuteJavaScript(
      web_view(),
      [NSString
          stringWithFormat:@"Object.defineProperty(window, 'devicePixelRatio', "
                           @"{value: %f, configurable: true}); true;",
                           kDpr]);

  NSDictionary* result = DragAndReleaseByCoordinates(
      from_x * kDpr, from_y * kDpr,
      static_cast<int>(Coordinate::PIXEL_TYPE_PHYSICAL_PIXELS), to_x * kDpr,
      to_y * kDpr, static_cast<int>(Coordinate::PIXEL_TYPE_PHYSICAL_PIXELS));
  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_NSEQ(result[@"resultCode"],
              @(static_cast<int>(DragAndReleaseToolResultCode::kOk)));
}

// Tests that dragging an HTML range slider updates its value.
TEST_F(DragAndReleaseToolJavaScriptTest, RangeSlider_UpdatesValue) {
  (void)web::test::ExecuteJavaScript(web_view(), @R"(
        window.range_events = [];
        const r = document.getElementById('range');
        r.addEventListener('input', () => window.range_events.push('input'));
        r.addEventListener('change', () => window.range_events.push('change'));
      )");

  NSDictionary* range_rect = GetElementClientRect("range");
  ASSERT_NE(range_rect, nil);
  double from_x = [range_rect[@"left"] doubleValue] + 5.0;
  double from_y = [range_rect[@"top"] doubleValue] +
                  [range_rect[@"height"] doubleValue] / 2.0;
  double to_x =
      [range_rect[@"left"] doubleValue] + [range_rect[@"width"] doubleValue];
  double to_y = from_y;

  NSDictionary* result = DragAndReleaseByCoordinates(
      from_x, from_y, static_cast<int>(Coordinate::PIXEL_TYPE_DIPS), to_x, to_y,
      static_cast<int>(Coordinate::PIXEL_TYPE_DIPS));
  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_NSEQ(result[@"resultCode"],
              @(static_cast<int>(DragAndReleaseToolResultCode::kOk)));

  id slider_value = web::test::ExecuteJavaScript(
      web_view(), @"document.getElementById('range').value");
  NSString* slider_value_str = base::apple::ObjCCast<NSString>(slider_value);
  ASSERT_NE(slider_value_str, nil);
  EXPECT_NSEQ(slider_value_str, @"100");

  id range_events = web::test::ExecuteJavaScript(
      web_view(), @"window.range_events.join(',')");
  NSString* range_events_str = base::apple::ObjCCast<NSString>(range_events);
  ASSERT_NE(range_events_str, nil);
  EXPECT_NSEQ(range_events_str, @"input,change");
}

// Tests that dragging an HTML range slider configured with step="any" updates
// correctly to continuous values.
TEST_F(DragAndReleaseToolJavaScriptTest, RangeSlider_StepAny) {
  (void)web::test::ExecuteJavaScript(web_view(), @R"(
        window.range_events = [];
        const r = document.getElementById('range');
        r.setAttribute('step', 'any');
        r.value = '0';
        r.addEventListener('input', () => window.range_events.push('input'));
        r.addEventListener('change', () => window.range_events.push('change'));
      )");

  NSDictionary* range_rect = GetElementClientRect("range");
  ASSERT_NE(range_rect, nil);
  double from_x = [range_rect[@"left"] doubleValue] + 5.0;
  double from_y = [range_rect[@"top"] doubleValue] +
                  [range_rect[@"height"] doubleValue] / 2.0;
  double to_x = [range_rect[@"left"] doubleValue] +
                [range_rect[@"width"] doubleValue] * 0.375;
  double to_y = from_y;

  NSDictionary* result = DragAndReleaseByCoordinates(
      from_x, from_y, static_cast<int>(Coordinate::PIXEL_TYPE_DIPS), to_x, to_y,
      static_cast<int>(Coordinate::PIXEL_TYPE_DIPS));
  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_NSEQ(result[@"resultCode"],
              @(static_cast<int>(DragAndReleaseToolResultCode::kOk)));

  id slider_value = web::test::ExecuteJavaScript(
      web_view(), @"document.getElementById('range').value");
  NSString* slider_value_str = base::apple::ObjCCast<NSString>(slider_value);
  ASSERT_NE(slider_value_str, nil);
  EXPECT_NSEQ(slider_value_str, @"37.5");

  id range_events = web::test::ExecuteJavaScript(
      web_view(), @"window.range_events.join(',')");
  NSString* range_events_str = base::apple::ObjCCast<NSString>(range_events);
  ASSERT_NE(range_events_str, nil);
  EXPECT_NSEQ(range_events_str, @"input,change");
}

// Tests that removing the element from the DOM mid-drag suppresses the drag and
// returns kDragSuppressed.
TEST_F(DragAndReleaseToolJavaScriptTest,
       DraggingElementDetached_DragSuppressed) {
  (void)web::test::ExecuteJavaScript(web_view(), @R"(
        document.getElementById('pointerLogger')
          .addEventListener('mousedown',
            e => { e.target.remove(); });
      )");

  int from_node_id = GetNodeId("pointerLogger");
  int to_node_id = GetNodeId("dragLogger");
  ASSERT_GT(from_node_id, 0);
  ASSERT_GT(to_node_id, 0);

  NSDictionary* result = DragAndReleaseByNodeIds(from_node_id, to_node_id);
  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_NSEQ(
      result[@"resultCode"],
      @(static_cast<int>(DragAndReleaseToolResultCode::kDragSuppressed)));
}

// Tests that dragging an element whose handlers call preventDefault() on drag
// or touch events does not suppress the drag action, as the tool executes the
// full simulated event lifecycle regardless of default prevention.
TEST_F(DragAndReleaseToolJavaScriptTest, PreventDefault_DoesNotSuppressDrag) {
  (void)web::test::ExecuteJavaScript(
      web_view(), @R"(document.getElementById('pointerLogger')
                  .addEventListener('dragstart', e => e.preventDefault());
          document.getElementById('pointerLogger')
                  .addEventListener('touchstart', e => e.preventDefault());
    )");

  int from_node_id = GetNodeId("pointerLogger");
  int to_node_id = GetNodeId("dragLogger");
  ASSERT_GT(from_node_id, 0);
  ASSERT_GT(to_node_id, 0);

  NSDictionary* result = DragAndReleaseByNodeIds(from_node_id, to_node_id);
  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_NSEQ(result[@"resultCode"],
              @(static_cast<int>(DragAndReleaseToolResultCode::kOk)));
}

// Tests that dragging an offscreen element fails with out-of-bounds, but
// succeeds after being scrolled into the viewport.
TEST_F(DragAndReleaseToolJavaScriptTest, OffscreenElementTarget) {
  int offscreen_id = GetNodeId("offscreenRange");
  ASSERT_GT(offscreen_id, 0);

  // Initially offscreen at top: 200vh.
  NSDictionary* offscreen_result =
      DragAndReleaseByNodeIds(offscreen_id, offscreen_id);
  ASSERT_NE(offscreen_result, nil);
  ASSERT_NE(offscreen_result[@"resultCode"], nil);
  EXPECT_NSEQ(offscreen_result[@"resultCode"],
              @(static_cast<int>(
                  DragAndReleaseToolResultCode::kFromCoordinatesOutOfBounds)));

  WKWebView* web_view = this->web_view();

  // Move the element into the viewport natively via DOM mutation to bypass
  // async multi-process scrolling issues on unparented WKWebViews.
  (void)web::test::ExecuteJavaScript(
      web_view,
      @"document.getElementById('offscreenRange').style.top = '200px';");

  // WebKit layout is asynchronous; wait until the element enters the viewport
  // before attempting to drag it.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return IsElementCenterInViewport(web_view, "offscreenRange");
      }));

  NSDictionary* visible_result =
      DragAndReleaseByNodeIds(offscreen_id, offscreen_id);
  ASSERT_NE(visible_result, nil);
  ASSERT_NE(visible_result[@"resultCode"], nil);
  EXPECT_NSEQ(visible_result[@"resultCode"],
              @(static_cast<int>(DragAndReleaseToolResultCode::kOk)))
      << "Failure message: "
      << base::SysNSStringToUTF8(
             base::apple::ObjCCast<NSString>(visible_result[@"message"]));
}

class DragAndReleaseToolJavaScriptParameterizedTest
    : public DragAndReleaseToolJavaScriptTest,
      public ::testing::WithParamInterface<std::tuple<CallType, CallType>> {
 protected:
  // Dispatches a dragAndRelease action between `from_element_id` and
  // `to_element_id` using either viewport coordinates or DOM node IDs
  // based on the parameterized source and destination target types.
  // Returns the result NSDictionary, or nil if element retrieval fails.
  NSDictionary* DragAndReleaseBetweenElements(
      const std::string& from_element_id,
      const std::string& to_element_id) {
    CallType from_call_type = std::get<0>(GetParam());
    CallType to_call_type = std::get<1>(GetParam());

    NSDictionary* from_target = nil;
    if (from_call_type == CallType::kByCoordinate) {
      NSDictionary* from_rect = GetElementClientRect(from_element_id);
      EXPECT_NE(from_rect, nil);
      if (!from_rect) {
        return nil;
      }
      double from_x = [from_rect[@"left"] doubleValue] +
                      [from_rect[@"width"] doubleValue] / 2.0;
      double from_y = [from_rect[@"top"] doubleValue] +
                      [from_rect[@"height"] doubleValue] / 2.0;
      from_target = @{
        @"coordinate" : @{
          @"x" : @(from_x),
          @"y" : @(from_y),
          @"pixelType" : @(static_cast<int>(Coordinate::PIXEL_TYPE_DIPS)),
        }
      };
    } else {
      int from_node_id = GetNodeId(from_element_id);
      EXPECT_GT(from_node_id, 0);
      if (from_node_id <= 0) {
        return nil;
      }
      from_target = @{@"contentNodeId" : @(from_node_id)};
    }

    NSDictionary* to_target = nil;
    if (to_call_type == CallType::kByCoordinate) {
      NSDictionary* to_rect = GetElementClientRect(to_element_id);
      EXPECT_NE(to_rect, nil);
      if (!to_rect) {
        return nil;
      }
      double to_x = [to_rect[@"left"] doubleValue] +
                    [to_rect[@"width"] doubleValue] / 2.0;
      double to_y = [to_rect[@"top"] doubleValue] +
                    [to_rect[@"height"] doubleValue] / 2.0;
      to_target = @{
        @"coordinate" : @{
          @"x" : @(to_x),
          @"y" : @(to_y),
          @"pixelType" : @(static_cast<int>(Coordinate::PIXEL_TYPE_DIPS)),
        }
      };
    } else {
      int to_node_id = GetNodeId(to_element_id);
      EXPECT_GT(to_node_id, 0);
      if (to_node_id <= 0) {
        return nil;
      }
      to_target = @{@"contentNodeId" : @(to_node_id)};
    }

    return DragAndRelease(from_target, to_target);
  }
};

// Tests that dragging between two valid elements by coordinate or node ID
// dispatches touch, mouse, and drag events successfully.
TEST_P(DragAndReleaseToolJavaScriptParameterizedTest, DragAndReleaseSuccess) {
  NSDictionary* result =
      DragAndReleaseBetweenElements("pointerLogger", "dragLogger");
  ASSERT_NE(result, nil);
  ASSERT_NE(result[@"resultCode"], nil);
  EXPECT_NSEQ(result[@"resultCode"],
              @(static_cast<int>(DragAndReleaseToolResultCode::kOk)));

  // Verify built-in event log from drag.html for mouse events.
  std::string built_in_event_log = GetEventLogString();
  EXPECT_FALSE(built_in_event_log.empty());
  EXPECT_THAT(built_in_event_log, testing::HasSubstr("mousemove["));
  EXPECT_THAT(built_in_event_log, testing::HasSubstr("mouseup["));

  // Verify extra structured event objects parsed into base::ListValue of
  // base::DictValue.
  std::optional<base::ListValue> events = GetDispatchedEvents();
  ASSERT_TRUE(events.has_value());
  EXPECT_FALSE(events->empty());

  // Verify mouse events carry bubbles: true.
  EXPECT_THAT(*events, testing::Contains(HasMouseEvent("mousedown")));
  EXPECT_THAT(*events, testing::Contains(HasMouseEvent("mousemove")));
  EXPECT_THAT(*events, testing::Contains(HasMouseEvent("mouseup")));

  // Verify drag events carry bubbles: true and cancelable: true.
  EXPECT_THAT(*events, testing::Contains(HasDragEvent("dragstart")));
  EXPECT_THAT(*events, testing::Contains(HasDragEvent("drag")));
  EXPECT_THAT(*events, testing::Contains(HasDragEvent("dragenter")));
  EXPECT_THAT(*events, testing::Contains(HasDragEvent("dragover")));
  EXPECT_THAT(*events, testing::Contains(HasDragEvent("dragleave")));
  EXPECT_THAT(*events, testing::Contains(HasDragEvent("drop")));
  EXPECT_THAT(*events, testing::Contains(HasDragEvent("dragend")));

  // Verify touch events are suppressed on draggable elements.
  constexpr int kExpectedTouchId = 0;
  EXPECT_THAT(*events, testing::Not(testing::Contains(
                           HasTouchEvent("touchstart", kExpectedTouchId))));
  EXPECT_THAT(*events, testing::Not(testing::Contains(
                           HasTouchEvent("touchmove", kExpectedTouchId))));
  EXPECT_THAT(*events, testing::Not(testing::Contains(
                           HasTouchEvent("touchend", kExpectedTouchId))));
}

INSTANTIATE_TEST_SUITE_P(
    AllTargetCombinations,
    DragAndReleaseToolJavaScriptParameterizedTest,
    ::testing::Combine(
        ::testing::Values(CallType::kByCoordinate, CallType::kByNodeId),
        ::testing::Values(CallType::kByCoordinate, CallType::kByNodeId)),
    [](const ::testing::TestParamInfo<std::tuple<CallType, CallType>>& info) {
      CallType from_call_type = std::get<0>(info.param);
      CallType to_call_type = std::get<1>(info.param);
      std::string from_str = (from_call_type == CallType::kByCoordinate)
                                 ? "FromCoordinate"
                                 : "FromNodeId";
      std::string to_str = (to_call_type == CallType::kByCoordinate)
                               ? "ToCoordinate"
                               : "ToNodeId";
      return from_str + "_" + to_str;
    });

}  // namespace
