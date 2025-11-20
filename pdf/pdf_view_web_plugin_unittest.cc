// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_web_plugin.h"

#include <stdint.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/paint/paint_canvas.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/cookies/site_for_cookies.h"
#include "pdf/accessibility_structs.h"
#include "pdf/buildflags.h"
#include "pdf/content_restriction.h"
#include "pdf/document_layout.h"
#include "pdf/mojom/pdf.mojom.h"
#include "pdf/paint_ready_rect.h"
#include "pdf/pdf_accessibility_data_handler.h"
#include "pdf/pdf_features.h"
#include "pdf/pdf_ink_annotation_mode.h"
#include "pdf/test/fake_annotation_agent_host.h"
#include "pdf/test/input_event_util.h"
#include "pdf/test/mock_web_associated_url_loader.h"
#include "pdf/test/mouse_event_builder.h"
#include "pdf/test/test_helpers.h"
#include "pdf/test/test_pdfium_engine.h"
#include "printing/metafile_skia.h"
#include "printing/units.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/loader/http_body_element_type.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_http_header_visitor.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/range/range.h"
#include "ui/latency/latency_info.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PDF_INK2)
#include "base/files/file_path.h"
#include "pdf/pdf_ink_brush.h"
#include "pdf/pdf_ink_metrics_handler.h"
#include "pdf/pdf_ink_module_client.h"
#include "pdf/test/pdf_ink_test_helpers.h"
#include "third_party/ink/src/ink/strokes/stroke.h"
#endif

using printing::kUnitConversionFactorPixelsToPoints;

namespace chrome_pdf {

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Pointwise;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SizeIs;

#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
constexpr uint8_t kSaveDataBuffer[] = {'b', 'u', 'f', 'f', 'e', 'r'};
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

#if BUILDFLAG(ENABLE_PDF_INK2)
constexpr char kPdfLoadedWithV2InkAnnotationsMetric[] =
    "PDF.LoadedWithV2InkAnnotations2";
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

// `kCanvasSize` needs to be big enough to hold plugin's snapshots during
// testing.
constexpr gfx::Size kCanvasSize(100, 100);

// Note: Make sure `kDefaultColor` is different from `kPaintColor` and the
// plugin's background color. This will help identify bitmap changes after
// painting.
constexpr SkColor kDefaultColor = SK_ColorGREEN;

constexpr SkColor kPaintColor = SK_ColorRED;

struct PaintParams {
  // The plugin container's device scale.
  float device_scale;

  // The window area in CSS pixels.
  gfx::Rect window_rect;

  // The target painting area on the canvas in CSS pixels.
  gfx::Rect paint_rect;

  // The expected clipped area to be filled with paint color. The clipped area
  // should be the intersection of `paint_rect` and `window_rect`.
  gfx::Rect expected_clipped_rect;
};

MATCHER(SearchStringResultEq, "") {
  PDFiumEngineClient::SearchStringResult l = std::get<0>(arg);
  PDFiumEngineClient::SearchStringResult r = std::get<1>(arg);
  return l.start_index == r.start_index && l.length == r.length;
}

MATCHER_P(IsExpectedImeKeyEvent, expected_text, "") {
  if (arg.GetType() != blink::WebInputEvent::Type::kChar) {
    return false;
  }

  const auto& event = static_cast<const blink::WebKeyboardEvent&>(arg);
  return event.GetModifiers() == blink::WebInputEvent::kNoModifiers &&
         event.windows_key_code == expected_text[0] &&
         event.native_key_code == expected_text[0] &&
         event.dom_code == static_cast<int>(ui::DomCode::NONE) &&
         event.dom_key == ui::DomKey::NONE && !event.is_system_key &&
         !event.is_browser_shortcut && event.text.data() == expected_text &&
         event.unmodified_text.data() == expected_text;
}

base::Value::Dict ParseMessage(std::string_view json) {
  return std::move(base::test::ParseJson(json).GetDict());
}

// Generates the expected `SkBitmap` with `paint_color` filled in the expected
// clipped area and `kDefaultColor` as the background color.
SkBitmap GenerateExpectedBitmapForPaint(const gfx::Rect& expected_clipped_rect,
                                        SkColor paint_color) {
  sk_sp<SkSurface> expected_surface =
      CreateSkiaSurfaceForTesting(kCanvasSize, kDefaultColor);
  expected_surface->getCanvas()->clipIRect(
      gfx::RectToSkIRect(expected_clipped_rect));
  expected_surface->getCanvas()->clear(paint_color);

  SkBitmap expected_bitmap;
  expected_surface->makeImageSnapshot()->asLegacyBitmap(&expected_bitmap);
  return expected_bitmap;
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
base::Value::Dict GenerateShowSearchifyInProgressMessage(bool show) {
  return base::Value::Dict()
      .Set("type", "showSearchifyInProgress")
      .Set("show", show);
}
#endif

class MockHeaderVisitor : public blink::WebHTTPHeaderVisitor {
 public:
  MOCK_METHOD(void,
              VisitHeader,
              (const blink::WebString&, const blink::WebString&),
              (override));
};

class MockPdfAccessibilityDataHandler : public PdfAccessibilityDataHandler {
 public:
  // PdfAccessibilityDataHandler:
  MOCK_METHOD(void,
              SetAccessibilityViewportInfo,
              (AccessibilityViewportInfo),
              (override));
  MOCK_METHOD(void,
              SetAccessibilityDocInfo,
              (std::unique_ptr<AccessibilityDocInfo>),
              (override));
  MOCK_METHOD(void,
              SetAccessibilityPageInfo,
              (AccessibilityPageInfo,
               std::vector<AccessibilityTextRunInfo>,
               std::vector<AccessibilityCharInfo>,
               AccessibilityPageObjects),
              (override));
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  MOCK_METHOD(void, OnHasSearchifyText, (), (override));
#endif
};

class FakePdfViewWebPluginClient : public PdfViewWebPlugin::Client {
 public:
  FakePdfViewWebPluginClient() {
    ON_CALL(*this, CreateAssociatedURLLoader).WillByDefault([]() {
      auto associated_loader =
          std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
      ON_CALL(*associated_loader, LoadAsynchronously)
          .WillByDefault([](const blink::WebURLRequest& /*request*/,
                            blink::WebAssociatedURLLoaderClient* client) {
            // TODO(crbug.com/40224475): Must trigger callback to free
            // `UrlLoader`.
            client->DidReceiveResponse(blink::WebURLResponse());
            client->DidFinishLoading();
          });
      return associated_loader;
    });
    ON_CALL(*this, GetIsolate).WillByDefault(Return(GetBlinkIsolate()));
    ON_CALL(*this, DeviceScaleFactor).WillByDefault(Return(1.0f));
    ON_CALL(*this, GetEmbedderOriginString)
        .WillByDefault(
            Return("chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/"));
    ON_CALL(*this, HasFrame).WillByDefault(Return(true));
  }

  // PdfViewWebPlugin::Client:
  MOCK_METHOD(std::unique_ptr<base::Value>,
              FromV8Value,
              (v8::Local<v8::Value>, v8::Local<v8::Context>),
              (override));

  MOCK_METHOD(base::WeakPtr<Client>, GetWeakPtr, (), (override));

  MOCK_METHOD(std::unique_ptr<PDFiumEngine>,
              CreateEngine,
              (PDFiumEngineClient*, PDFiumFormFiller::ScriptOption),
              (override));

  MOCK_METHOD(void,
              SetPluginContainer,
              (blink::WebPluginContainer*),
              (override));
  MOCK_METHOD(blink::WebPluginContainer*, PluginContainer, (), (override));

  MOCK_METHOD(v8::Isolate*, GetIsolate, (), (override));

  MOCK_METHOD(net::SiteForCookies, SiteForCookies, (), (const override));

  MOCK_METHOD(blink::WebURL,
              CompleteURL,
              (const blink::WebString&),
              (const override));

  MOCK_METHOD(void, PostMessage, (base::Value::Dict), (override));

  MOCK_METHOD(void, Invalidate, (), (override));

  MOCK_METHOD(void,
              RequestTouchEventType,
              (blink::WebPluginContainer::TouchEventRequestType),
              (override));

  MOCK_METHOD(void, ReportFindInPageMatchCount, (int, int, bool), (override));

  MOCK_METHOD(void, ReportFindInPageSelection, (int, int, bool), (override));

  MOCK_METHOD(void,
              ReportFindInPageTickmarks,
              (const std::vector<gfx::Rect>&),
              (override));

  MOCK_METHOD(float, DeviceScaleFactor, (), (override));

  MOCK_METHOD(gfx::PointF, GetScrollPosition, (), (override));

  MOCK_METHOD(void, UsePluginAsFindHandler, (), (override));

  MOCK_METHOD(void,
              SetReferrerForRequest,
              (blink::WebURLRequest&, const blink::WebURL&),
              (override));

  MOCK_METHOD(void, Alert, (const blink::WebString&), (override));

  MOCK_METHOD(bool, Confirm, (const blink::WebString&), (override));

  MOCK_METHOD(blink::WebString,
              Prompt,
              (const blink::WebString&, const blink::WebString&),
              (override));

  MOCK_METHOD(void,
              TextSelectionChanged,
              (const blink::WebString&, uint32_t, const gfx::Range&),
              (override));

  MOCK_METHOD(std::unique_ptr<blink::WebAssociatedURLLoader>,
              CreateAssociatedURLLoader,
              (const blink::WebAssociatedURLLoaderOptions&),
              (override));

  MOCK_METHOD(void, UpdateTextInputState, (), (override));

  MOCK_METHOD(void, UpdateSelectionBounds, (), (override));

  MOCK_METHOD(std::string, GetEmbedderOriginString, (), (override));

  MOCK_METHOD(bool, HasFrame, (), (const override));

  MOCK_METHOD(void, DidStartLoading, (), (override));
  MOCK_METHOD(void, DidStopLoading, (), (override));

  MOCK_METHOD(void, RecordComputedAction, (const std::string&), (override));

  MOCK_METHOD(std::unique_ptr<PdfAccessibilityDataHandler>,
              CreateAccessibilityDataHandler,
              (PdfAccessibilityActionHandler*, blink::WebPluginContainer*),
              (override));
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  MOCK_METHOD(void,
              GetOcrMaxImageDimension,
              (base::OnceCallback<void(uint32_t)> callback),
              (override));

  MOCK_METHOD(void,
              PerformOcr,
              (const SkBitmap& image,
               base::OnceCallback<void(screen_ai::mojom::VisualAnnotationPtr)>
                   callback),
              (override));

  MOCK_METHOD(void,
              SetOcrDisconnectedCallback,
              (base::RepeatingClosure callback),
              (override));
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
};

class FakePdfHost : public pdf::mojom::PdfHost {
 public:
  MOCK_METHOD(void,
              SetListener,
              (mojo::PendingRemote<pdf::mojom::PdfListener>),
              (override));
  MOCK_METHOD(void, OnDocumentLoadComplete, (), (override));
  MOCK_METHOD(void, UpdateContentRestrictions, (int32_t), (override));
  MOCK_METHOD(void,
              SaveUrlAs,
              (const GURL&, network::mojom::ReferrerPolicy),
              (override));
  MOCK_METHOD(void,
              SelectionChanged,
              (const gfx::PointF&, int32_t, const gfx::PointF&, int32_t),
              (override));
  MOCK_METHOD(void, SetPluginCanSave, (bool), (override));
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  MOCK_METHOD(void, OnSearchifyStarted, (), (override));
#endif
};

}  // namespace

class PdfViewWebPluginWithoutInitializeTest : public testing::Test {
 protected:
  // Custom deleter for `plugin_`. PdfViewWebPlugin must be destroyed by
  // PdfViewWebPlugin::Destroy() instead of its destructor.
  struct PluginDeleter {
    void operator()(PdfViewWebPlugin* ptr) { ptr->Destroy(); }
  };

  static void AddToPluginParams(std::string_view name,
                                std::string_view value,
                                blink::WebPluginParams& params) {
    params.attribute_names.push_back(blink::WebString::FromUTF8(name));
    params.attribute_values.push_back(blink::WebString::FromUTF8(value));
  }

  void SetUpPlugin(std::string_view document_url,
                   blink::WebPluginParams params) {
    auto client = std::make_unique<NiceMock<FakePdfViewWebPluginClient>>();
    client_ptr_ = client.get();

    ON_CALL(*client_ptr_, CompleteURL)
        .WillByDefault([parsed_document_url = GURL(document_url)](
                           const blink::WebString& partial_url) {
          return parsed_document_url.Resolve(partial_url.Utf8());
        });
    ON_CALL(*client_ptr_, CreateEngine)
        .WillByDefault([this](
                           PDFiumEngineClient* client,
                           PDFiumFormFiller::ScriptOption /*script_option*/) {
          auto engine = std::make_unique<NiceMock<TestPDFiumEngine>>(client);
          engine_ptr_ = engine.get();
          return engine;
        });
    ON_CALL(*client_ptr_, CreateAccessibilityDataHandler)
        .WillByDefault([this]() {
          auto handler =
              std::make_unique<NiceMock<MockPdfAccessibilityDataHandler>>();
          accessibility_data_handler_ptr_ = handler.get();
          return handler;
        });
    SetUpClient();

    plugin_ =
        std::unique_ptr<PdfViewWebPlugin, PluginDeleter>(new PdfViewWebPlugin(
            std::move(client),
            mojo::AssociatedRemote<pdf::mojom::PdfHost>(
                pdf_receiver_.BindNewEndpointAndPassDedicatedRemote()),
            std::move(params)));
  }

  void SetUpPluginWithUrl(const std::string& url) {
    blink::WebPluginParams params;
    AddToPluginParams("src", url, params);
    SetUpPluginParams(params);

    SetUpPlugin(url, std::move(params));
  }

  // Allows derived classes to customize plugin parameters within
  // `SetUpPluginWithUrl()`.
  virtual void SetUpPluginParams(blink::WebPluginParams& params) {}

  // Allows derived classes to customize `client_ptr_` within `SetUpPlugin()`.
  virtual void SetUpClient() {}

  void ExpectUpdateTextInputState(
      blink::WebTextInputType expected_text_input_type) {
    EXPECT_CALL(*client_ptr_, UpdateTextInputState)
        .WillOnce([this, expected_text_input_type]() {
          EXPECT_EQ(expected_text_input_type,
                    plugin_->GetPluginTextInputType());
        });
  }

  void OnMessageWithEngineUpdate(const base::Value::Dict& message) {
    // New engine will be created making this unowned reference stale.
    engine_ptr_ = nullptr;
    plugin_->OnMessage(message);
  }

  NiceMock<FakePdfHost> pdf_host_;
  mojo::AssociatedReceiver<pdf::mojom::PdfHost> pdf_receiver_{&pdf_host_};

  // Must outlive raw_ptrs below.
  std::unique_ptr<PdfViewWebPlugin, PluginDeleter> plugin_;

  raw_ptr<FakePdfViewWebPluginClient> client_ptr_;
  raw_ptr<TestPDFiumEngine> engine_ptr_;
  raw_ptr<MockPdfAccessibilityDataHandler> accessibility_data_handler_ptr_;
};

class PdfViewWebPluginTest : public PdfViewWebPluginWithoutInitializeTest {
 protected:
  static constexpr char kPdfUrl[] = "http://localhost/example.pdf";

  void SetUp() override {
    SetUpPluginWithUrl(kPdfUrl);

    EXPECT_TRUE(plugin_->InitializeForTesting());
  }

  void SetDocumentDimensions(const gfx::Size& dimensions) {
    EXPECT_CALL(*engine_ptr_, ApplyDocumentLayout)
        .WillRepeatedly(Return(dimensions));
    SendViewportMessage(/*zoom=*/1.0);
  }

  void SendViewportMessage(double zoom) {
    base::Value::Dict message = ParseMessage(R"({
      "type": "viewport",
      "userInitiated": false,
      "zoom": 1,
      "layoutOptions": {
        "direction": 2,
        "defaultPageOrientation": 0,
        "twoUpViewEnabled": false,
      },
      "xOffset": 0,
      "yOffset": 0,
      "pinchPhase": 0,
    })");
    message.Set("zoom", zoom);
    plugin_->OnMessage(message);
  }

  void UpdatePluginGeometry(float device_scale, const gfx::Rect& window_rect) {
    UpdatePluginGeometryWithoutWaiting(device_scale, window_rect);

    // Waits for main thread callback scheduled by `PaintManager`.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  void UpdatePluginGeometryWithoutWaiting(float device_scale,
                                          const gfx::Rect& window_rect) {
    // The plugin container's device scale must be set before calling
    // UpdateGeometry().
    EXPECT_CALL(*client_ptr_, DeviceScaleFactor)
        .WillRepeatedly(Return(device_scale));
    plugin_->UpdateGeometry(window_rect, window_rect, window_rect,
                            /*is_visible=*/true);
  }

  void TestUpdateGeometrySetsPluginRect(float device_scale,
                                        const gfx::Rect& window_rect,
                                        float expected_device_scale,
                                        const gfx::Rect& expected_plugin_rect) {
    UpdatePluginGeometryWithoutWaiting(device_scale, window_rect);
    EXPECT_EQ(expected_device_scale, plugin_->GetDeviceScaleForTesting())
        << "Device scale comparison failure at device scale of "
        << device_scale;
    EXPECT_EQ(expected_plugin_rect, plugin_->GetPluginRectForTesting())
        << "Plugin rect comparison failure at device scale of " << device_scale
        << ", window rect of " << window_rect.ToString();
  }

  void TestPaintEmptySnapshots(float device_scale,
                               const gfx::Rect& window_rect,
                               const gfx::Rect& paint_rect,
                               const gfx::Rect& expected_clipped_rect) {
    UpdatePluginGeometryWithoutWaiting(device_scale, window_rect);
    canvas_.DrawColor(kDefaultColor);

    plugin_->Paint(canvas_.sk_canvas(), paint_rect);

    // Expect the clipped area on canvas to be filled with plugin's background
    // color.
    SkBitmap expected_bitmap = GenerateExpectedBitmapForPaint(
        expected_clipped_rect, plugin_->GetBackgroundColor());
    EXPECT_TRUE(cc::MatchesBitmap(canvas_.GetBitmap(), expected_bitmap,
                                  cc::ExactPixelComparator()))
        << "Failure at device scale of " << device_scale << ", window rect of "
        << window_rect.ToString();
  }

  void TestPaintSnapshots(float device_scale,
                          const gfx::Rect& window_rect,
                          const gfx::Rect& paint_rect,
                          const gfx::Rect& expected_clipped_rect) {
    UpdatePluginGeometry(device_scale, window_rect);
    canvas_.DrawColor(kDefaultColor);

    // Paint the plugin with `kPaintColor`.
    plugin_->UpdateSnapshot(CreateSkiaImageForTesting(
        plugin_->GetPluginRectForTesting().size(), kPaintColor));
    plugin_->Paint(canvas_.sk_canvas(), paint_rect);

    // Expect the clipped area on canvas to be filled with `kPaintColor`.
    SkBitmap expected_bitmap =
        GenerateExpectedBitmapForPaint(expected_clipped_rect, kPaintColor);
    EXPECT_TRUE(cc::MatchesBitmap(canvas_.GetBitmap(), expected_bitmap,
                                  cc::ExactPixelComparator()))
        << "Failure at device scale of " << device_scale << ", window rect of "
        << window_rect.ToString();
  }

  ui::Cursor TestSendInputEvent(const blink::WebInputEvent& event,
                                blink::WebInputEventResult expected_result) {
    ui::Cursor cursor;
    EXPECT_EQ(
        expected_result,
        plugin_->HandleInputEvent(
            blink::WebCoalescedInputEvent(event, ui::LatencyInfo()), &cursor));
    return cursor;
  }

  // Provides the cc::PaintCanvas for painting.
  gfx::Canvas canvas_{kCanvasSize, /*image_scale=*/1.0f, /*is_opaque=*/true};
};

class PdfViewWebPluginFullFrameTest : public PdfViewWebPluginTest {
 protected:
  void SetUpPluginParams(blink::WebPluginParams& params) override {
    AddToPluginParams("full-frame", "full-frame", params);
  }
};

TEST_F(PdfViewWebPluginWithoutInitializeTest, Initialize) {
  SetUpPluginWithUrl("http://localhost/example.pdf");

  EXPECT_CALL(*client_ptr_, CreateAssociatedURLLoader)
      .WillOnce([](const blink::WebAssociatedURLLoaderOptions& options) {
        EXPECT_TRUE(options.grant_universal_access);

        auto associated_loader =
            std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
        EXPECT_CALL(*associated_loader, LoadAsynchronously)
            .WillOnce([](const blink::WebURLRequest& request,
                         blink::WebAssociatedURLLoaderClient* client) {
              EXPECT_EQ("http://localhost/example.pdf",
                        request.Url().GetString().Utf8());
              EXPECT_EQ("GET", request.HttpMethod().Utf8());
              EXPECT_TRUE(request.HttpBody().IsNull());

              NiceMock<MockHeaderVisitor> header_visitor;
              EXPECT_CALL(header_visitor, VisitHeader).Times(0);
              request.VisitHttpHeaderFields(&header_visitor);

              EXPECT_FALSE(client->WillFollowRedirect(blink::WebURL(),
                                                      blink::WebURLResponse()));
              client->DidReceiveResponse(blink::WebURLResponse());
              client->DidFinishLoading();
            });
        return associated_loader;
      });
  EXPECT_CALL(*client_ptr_, SetReferrerForRequest).Times(0);

  EXPECT_TRUE(plugin_->InitializeForTesting());
}

TEST_F(PdfViewWebPluginWithoutInitializeTest, InitializeWithEmptyUrl) {
  SetUpPluginWithUrl("");

  EXPECT_CALL(*client_ptr_, CreateAssociatedURLLoader).Times(0);

  EXPECT_FALSE(plugin_->InitializeForTesting());
}

TEST_F(PdfViewWebPluginWithoutInitializeTest, InitializeForPrintPreview) {
  SetUpPluginWithUrl("about:blank");

  EXPECT_CALL(*client_ptr_, GetEmbedderOriginString)
      .WillRepeatedly(Return("chrome://print/"));
  EXPECT_CALL(*client_ptr_, CreateAssociatedURLLoader).Times(0);

  EXPECT_TRUE(plugin_->InitializeForTesting());
}

TEST_F(PdfViewWebPluginTest, CreateUrlLoader) {
  EXPECT_CALL(*client_ptr_, DidStartLoading).Times(0);
  EXPECT_CALL(pdf_host_, UpdateContentRestrictions).Times(0);
  plugin_->CreateUrlLoader();

  EXPECT_EQ(PdfViewWebPlugin::DocumentLoadState::kLoading,
            plugin_->document_load_state_for_testing());
  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginFullFrameTest, CreateUrlLoader) {
  EXPECT_CALL(*client_ptr_, DidStartLoading);
  EXPECT_CALL(pdf_host_, UpdateContentRestrictions(kContentRestrictionSave |
                                                   kContentRestrictionPrint));
  plugin_->CreateUrlLoader();

  EXPECT_EQ(PdfViewWebPlugin::DocumentLoadState::kLoading,
            plugin_->document_load_state_for_testing());
  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginFullFrameTest, CreateUrlLoaderMultipleTimes) {
  plugin_->CreateUrlLoader();

  EXPECT_CALL(*client_ptr_, DidStartLoading).Times(0);
  plugin_->CreateUrlLoader();
}

TEST_F(PdfViewWebPluginFullFrameTest, CreateUrlLoaderAfterDocumentLoadFailed) {
  plugin_->CreateUrlLoader();
  plugin_->DocumentLoadFailed();

  EXPECT_CALL(*client_ptr_, DidStartLoading);
  plugin_->CreateUrlLoader();
}

TEST_F(PdfViewWebPluginTest, DocumentLoadComplete) {
  plugin_->CreateUrlLoader();

  EXPECT_CALL(*client_ptr_, RecordComputedAction("PDF.LoadSuccess"));
  EXPECT_CALL(*client_ptr_, PostMessage);
  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(R"({
    "type": "formFocusChange",
    "focused": "none",
  })")));
  ExpectUpdateTextInputState(blink::WebTextInputType::kWebTextInputTypeNone);
  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(R"({
    "type": "printPreviewLoaded",
  })")))
      .Times(0);
  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo)
      .Times(0);
  EXPECT_CALL(*client_ptr_, DidStopLoading).Times(0);
  EXPECT_CALL(pdf_host_, UpdateContentRestrictions).Times(0);
  plugin_->DocumentLoadComplete();

  EXPECT_EQ(PdfViewWebPlugin::DocumentLoadState::kComplete,
            plugin_->document_load_state_for_testing());
  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginFullFrameTest, DocumentLoadComplete) {
  // Must flush IPCs after `CreateUrlLoader()` in full-frame mode, otherwise
  // there's an unexpected `UpdateContentRestrictions()` call (see the
  // `PdfViewWebPluginFullFrameTest.CreateUrlLoader` test).
  plugin_->CreateUrlLoader();
  pdf_receiver_.FlushForTesting();

  EXPECT_CALL(*client_ptr_, RecordComputedAction("PDF.LoadSuccess"));
  EXPECT_CALL(*client_ptr_, PostMessage);
  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(R"({
    "type": "formFocusChange",
    "focused": "none",
  })")));
  ExpectUpdateTextInputState(blink::WebTextInputType::kWebTextInputTypeNone);
  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(R"({
    "type": "printPreviewLoaded",
  })")))
      .Times(0);
  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo)
      .Times(0);
  EXPECT_CALL(*client_ptr_, DidStopLoading);
  EXPECT_CALL(pdf_host_, UpdateContentRestrictions(kContentRestrictionPrint |
                                                   kContentRestrictionPaste |
                                                   kContentRestrictionCut |
                                                   kContentRestrictionCopy));
  EXPECT_CALL(pdf_host_, OnDocumentLoadComplete);
  plugin_->DocumentLoadComplete();

  EXPECT_EQ(PdfViewWebPlugin::DocumentLoadState::kComplete,
            plugin_->document_load_state_for_testing());
  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginTest, DocumentLoadFailed) {
  plugin_->CreateUrlLoader();

  EXPECT_CALL(*client_ptr_, RecordComputedAction("PDF.LoadFailure"));
  EXPECT_CALL(*client_ptr_, DidStopLoading).Times(0);
  EXPECT_CALL(pdf_host_, OnDocumentLoadComplete).Times(0);
  plugin_->DocumentLoadFailed();

  EXPECT_EQ(PdfViewWebPlugin::DocumentLoadState::kFailed,
            plugin_->document_load_state_for_testing());
}

TEST_F(PdfViewWebPluginFullFrameTest, DocumentLoadFailed) {
  plugin_->CreateUrlLoader();

  EXPECT_CALL(*client_ptr_, RecordComputedAction("PDF.LoadFailure"));
  EXPECT_CALL(*client_ptr_, DidStopLoading);
  plugin_->DocumentLoadFailed();

  EXPECT_EQ(PdfViewWebPlugin::DocumentLoadState::kFailed,
            plugin_->document_load_state_for_testing());
}

TEST_F(PdfViewWebPluginTest, DocumentHasUnsupportedFeature) {
  EXPECT_CALL(*client_ptr_, RecordComputedAction).Times(AnyNumber());
  EXPECT_CALL(*client_ptr_, RecordComputedAction("PDF_Unsupported_feature1"));
  EXPECT_CALL(*client_ptr_, RecordComputedAction("PDF_Unsupported_feature2"));

  plugin_->DocumentHasUnsupportedFeature("feature1");
  plugin_->DocumentHasUnsupportedFeature("feature2");

  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginTest, DocumentHasUnsupportedFeatureWithRepeatedFeature) {
  // Metrics should only be recorded once per feature.
  EXPECT_CALL(*client_ptr_, RecordComputedAction).Times(AnyNumber());
  EXPECT_CALL(*client_ptr_, RecordComputedAction("PDF_Unsupported_feature"));

  plugin_->DocumentHasUnsupportedFeature("feature");
  plugin_->DocumentHasUnsupportedFeature("feature");

  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginFullFrameTest, DocumentHasUnsupportedFeature) {
  EXPECT_CALL(*client_ptr_, RecordComputedAction).Times(AnyNumber());
  EXPECT_CALL(*client_ptr_, RecordComputedAction("PDF_Unsupported_feature1"));
  EXPECT_CALL(*client_ptr_, RecordComputedAction("PDF_Unsupported_feature2"));

  plugin_->DocumentHasUnsupportedFeature("feature1");
  plugin_->DocumentHasUnsupportedFeature("feature2");

  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginFullFrameTest,
       DocumentHasUnsupportedFeatureWithRepeatedFeature) {
  // Metrics should only be recorded once per feature.
  EXPECT_CALL(*client_ptr_, RecordComputedAction).Times(AnyNumber());
  EXPECT_CALL(*client_ptr_, RecordComputedAction("PDF_Unsupported_feature"));

  plugin_->DocumentHasUnsupportedFeature("feature");
  plugin_->DocumentHasUnsupportedFeature("feature");

  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginTest, DocumentLoadProgress) {
  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(R"({
    "type": "loadProgress",
    "progress": 5.0,
  })")));
  plugin_->DocumentLoadProgress(10, 200);
}

TEST_F(PdfViewWebPluginTest, DocumentLoadProgressIgnoreSmall) {
  plugin_->DocumentLoadProgress(2, 100);

  EXPECT_CALL(*client_ptr_, PostMessage).Times(0);
  plugin_->DocumentLoadProgress(3, 100);
}

TEST_F(PdfViewWebPluginTest, DocumentLoadProgressMultipleSmall) {
  plugin_->DocumentLoadProgress(2, 100);

  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(R"({
    "type": "loadProgress",
    "progress": 4.0,
  })")));
  plugin_->DocumentLoadProgress(3, 100);
  plugin_->DocumentLoadProgress(4, 100);
}

TEST_F(PdfViewWebPluginTest, EnableAccessibilityBeforeDocumentLoadComplete) {
  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo)
      .Times(0);
  plugin_->EnableAccessibility();

  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo);
  plugin_->CreateUrlLoader();
  plugin_->DocumentLoadComplete();
}

TEST_F(PdfViewWebPluginTest,
       EnableAccessibilityBeforeDocumentLoadCompleteRepeated) {
  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo)
      .Times(0);
  plugin_->EnableAccessibility();
  plugin_->EnableAccessibility();

  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo);
  plugin_->CreateUrlLoader();
  plugin_->DocumentLoadComplete();
}

TEST_F(PdfViewWebPluginTest, EnableAccessibilityAfterDocumentLoadComplete) {
  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo)
      .Times(0);
  plugin_->CreateUrlLoader();
  plugin_->DocumentLoadComplete();

  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo);
  plugin_->EnableAccessibility();
}

TEST_F(PdfViewWebPluginTest,
       EnableAccessibilityAfterDocumentLoadCompleteRepeated) {
  plugin_->CreateUrlLoader();
  plugin_->DocumentLoadComplete();
  plugin_->EnableAccessibility();

  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo)
      .Times(0);
  plugin_->EnableAccessibility();
}

TEST_F(PdfViewWebPluginTest,
       LoadOrReloadAccessibilityBeforeDocumentLoadComplete) {
  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo)
      .Times(0);
  plugin_->LoadOrReloadAccessibility();

  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo);
  plugin_->CreateUrlLoader();
  plugin_->DocumentLoadComplete();
}

TEST_F(PdfViewWebPluginTest,
       LoadOrReloadAccessibilityBeforeDocumentLoadCompleteRepeated) {
  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo)
      .Times(0);
  plugin_->LoadOrReloadAccessibility();
  plugin_->LoadOrReloadAccessibility();

  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo);
  plugin_->CreateUrlLoader();
  plugin_->DocumentLoadComplete();
}

TEST_F(PdfViewWebPluginTest,
       LoadOrReloadAccessibilityAfterDocumentLoadComplete) {
  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo)
      .Times(0);
  plugin_->CreateUrlLoader();
  plugin_->DocumentLoadComplete();

  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo);
  plugin_->LoadOrReloadAccessibility();
}

TEST_F(PdfViewWebPluginTest,
       LoadOrReloadAccessibilityAfterDocumentLoadCompleteRepeated) {
  plugin_->CreateUrlLoader();
  plugin_->DocumentLoadComplete();
  plugin_->LoadOrReloadAccessibility();

  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo);
  plugin_->LoadOrReloadAccessibility();
}

TEST_F(PdfViewWebPluginTest,
       LoadOrReloadAccessibilityResetsAccessibilityPageIndex) {
  plugin_->CreateUrlLoader();
  plugin_->DocumentLoadComplete();
  plugin_->LoadOrReloadAccessibility();
  EXPECT_EQ(plugin_->next_accessibility_page_index_for_testing(), 0);
  plugin_->set_next_accessibility_page_index_for_testing(5);

  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo);
  plugin_->LoadOrReloadAccessibility();
  EXPECT_EQ(plugin_->next_accessibility_page_index_for_testing(), 0);
}

TEST_F(PdfViewWebPluginTest, GetContentRestrictionsWithNoPermissions) {
  EXPECT_EQ(kContentRestrictionCopy | kContentRestrictionCut |
                kContentRestrictionPaste | kContentRestrictionPrint,
            plugin_->GetContentRestrictionsForTesting());
  EXPECT_FALSE(plugin_->CanCopy());
}

TEST_F(PdfViewWebPluginTest, GetContentRestrictionsWithCopyAllowed) {
  EXPECT_CALL(*engine_ptr_, HasPermission).WillRepeatedly(Return(false));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kCopy))
      .WillRepeatedly(Return(true));

  EXPECT_EQ(kContentRestrictionCut | kContentRestrictionPaste |
                kContentRestrictionPrint,
            plugin_->GetContentRestrictionsForTesting());
  EXPECT_TRUE(plugin_->CanCopy());
}

TEST_F(PdfViewWebPluginTest, GetContentRestrictionsWithPrintLowQualityAllowed) {
  EXPECT_CALL(*engine_ptr_, HasPermission).WillRepeatedly(Return(false));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kPrintLowQuality))
      .WillRepeatedly(Return(true));

  EXPECT_EQ(kContentRestrictionCopy | kContentRestrictionCut |
                kContentRestrictionPaste,
            plugin_->GetContentRestrictionsForTesting());
}

TEST_F(PdfViewWebPluginTest,
       GetContentRestrictionsWithCopyAndPrintLowQualityAllowed) {
  EXPECT_CALL(*engine_ptr_, HasPermission).WillRepeatedly(Return(false));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kCopy))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kPrintLowQuality))
      .WillRepeatedly(Return(true));

  EXPECT_EQ(kContentRestrictionCut | kContentRestrictionPaste,
            plugin_->GetContentRestrictionsForTesting());
}

TEST_F(PdfViewWebPluginTest, GetContentRestrictionsWithPrintAllowed) {
  EXPECT_CALL(*engine_ptr_, HasPermission).WillRepeatedly(Return(false));
  EXPECT_CALL(*engine_ptr_,
              HasPermission(DocumentPermission::kPrintHighQuality))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kPrintLowQuality))
      .WillRepeatedly(Return(true));

  EXPECT_EQ(kContentRestrictionCopy | kContentRestrictionCut |
                kContentRestrictionPaste,
            plugin_->GetContentRestrictionsForTesting());
}

TEST_F(PdfViewWebPluginTest, GetContentRestrictionsWithCopyAndPrintAllowed) {
  EXPECT_CALL(*engine_ptr_, HasPermission).WillRepeatedly(Return(false));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kCopy))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*engine_ptr_,
              HasPermission(DocumentPermission::kPrintHighQuality))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kPrintLowQuality))
      .WillRepeatedly(Return(true));

  EXPECT_EQ(kContentRestrictionCut | kContentRestrictionPaste,
            plugin_->GetContentRestrictionsForTesting());
}

TEST_F(PdfViewWebPluginTest, GetAccessibilityDocInfoWithNoPermissions) {
  std::unique_ptr<AccessibilityDocInfo> doc_info =
      plugin_->GetAccessibilityDocInfoForTesting();

  EXPECT_EQ(TestPDFiumEngine::kPageNumber, doc_info->page_count);
  EXPECT_FALSE(doc_info->is_tagged);
  EXPECT_FALSE(doc_info->text_accessible);
  EXPECT_FALSE(doc_info->text_copyable);
}

TEST_F(PdfViewWebPluginTest, GetAccessibilityDocInfoWithPDFDocTagged) {
  base::test::ScopedFeatureList scoped_feature_list(features::kPdfTags);
  EXPECT_CALL(*engine_ptr_, IsPDFDocTagged).WillRepeatedly(Return(true));

  std::unique_ptr<AccessibilityDocInfo> doc_info =
      plugin_->GetAccessibilityDocInfoForTesting();

  EXPECT_EQ(TestPDFiumEngine::kPageNumber, doc_info->page_count);
  EXPECT_TRUE(doc_info->is_tagged);
  EXPECT_FALSE(doc_info->text_accessible);
  EXPECT_FALSE(doc_info->text_copyable);
}

TEST_F(PdfViewWebPluginTest, GetAccessibilityDocInfoWithCopyAccessibleAllowed) {
  EXPECT_CALL(*engine_ptr_, HasPermission).WillRepeatedly(Return(false));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kCopyAccessible))
      .WillRepeatedly(Return(true));

  std::unique_ptr<AccessibilityDocInfo> doc_info =
      plugin_->GetAccessibilityDocInfoForTesting();

  EXPECT_EQ(TestPDFiumEngine::kPageNumber, doc_info->page_count);
  EXPECT_FALSE(doc_info->is_tagged);
  EXPECT_TRUE(doc_info->text_accessible);
  EXPECT_FALSE(doc_info->text_copyable);
}

TEST_F(PdfViewWebPluginTest, GetAccessibilityDocInfoWithCopyAllowed) {
  EXPECT_CALL(*engine_ptr_, HasPermission).WillRepeatedly(Return(false));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kCopy))
      .WillRepeatedly(Return(true));

  std::unique_ptr<AccessibilityDocInfo> doc_info =
      plugin_->GetAccessibilityDocInfoForTesting();

  EXPECT_EQ(TestPDFiumEngine::kPageNumber, doc_info->page_count);
  EXPECT_FALSE(doc_info->is_tagged);
  EXPECT_FALSE(doc_info->text_accessible);
  EXPECT_TRUE(doc_info->text_copyable);
}

TEST_F(PdfViewWebPluginTest,
       GetAccessibilityDocInfoWithCopyAndCopyAccessibleAllowed) {
  EXPECT_CALL(*engine_ptr_, HasPermission).WillRepeatedly(Return(false));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kCopy))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kCopyAccessible))
      .WillRepeatedly(Return(true));

  std::unique_ptr<AccessibilityDocInfo> doc_info =
      plugin_->GetAccessibilityDocInfoForTesting();

  EXPECT_EQ(TestPDFiumEngine::kPageNumber, doc_info->page_count);
  EXPECT_FALSE(doc_info->is_tagged);
  EXPECT_TRUE(doc_info->text_accessible);
  EXPECT_TRUE(doc_info->text_copyable);
}

TEST_F(
    PdfViewWebPluginTest,
    GetAccessibilityDocInfoWithPDFDocTaggedAndPDFCopyAndCopyAccessibleAllowed) {
  base::test::ScopedFeatureList scoped_feature_list(features::kPdfTags);
  EXPECT_CALL(*engine_ptr_, IsPDFDocTagged).WillRepeatedly(Return(true));
  EXPECT_CALL(*engine_ptr_, HasPermission).WillRepeatedly(Return(false));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kCopy))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kCopyAccessible))
      .WillRepeatedly(Return(true));

  std::unique_ptr<AccessibilityDocInfo> doc_info =
      plugin_->GetAccessibilityDocInfoForTesting();

  EXPECT_EQ(TestPDFiumEngine::kPageNumber, doc_info->page_count);
  EXPECT_TRUE(doc_info->is_tagged);
  EXPECT_TRUE(doc_info->text_accessible);
  EXPECT_TRUE(doc_info->text_copyable);
}

TEST_F(PdfViewWebPluginTest, UpdateGeometrySetsPluginRect) {
  EXPECT_CALL(*engine_ptr_, ZoomUpdated(2.0f));
  TestUpdateGeometrySetsPluginRect(
      /*device_scale=*/2.0f, /*window_rect=*/gfx::Rect(4, 4, 12, 12),
      /*expected_device_scale=*/2.0f,
      /*expected_plugin_rect=*/gfx::Rect(4, 4, 12, 12));
}

TEST_F(PdfViewWebPluginTest,
       UpdateGeometrySetsPluginRectOnVariousDeviceScales) {
  struct UpdateGeometryParams {
    // The plugin container's device scale.
    float device_scale;

    //  The window rect in CSS pixels.
    gfx::Rect window_rect;

    // The expected plugin device scale.
    float expected_device_scale;

    // The expected plugin rect in device pixels.
    gfx::Rect expected_plugin_rect;
  };

  static constexpr UpdateGeometryParams kUpdateGeometryParams[] = {
      {1.0f, gfx::Rect(3, 4, 5, 6), 1.0f, gfx::Rect(3, 4, 5, 6)},
      {2.0f, gfx::Rect(3, 4, 5, 6), 2.0f, gfx::Rect(3, 4, 5, 6)},
  };

  for (const auto& params : kUpdateGeometryParams) {
    TestUpdateGeometrySetsPluginRect(params.device_scale, params.window_rect,
                                     params.expected_device_scale,
                                     params.expected_plugin_rect);
  }
}

TEST_F(PdfViewWebPluginTest, UpdateGeometrySetsPluginRectWithEmptyWindow) {
  EXPECT_CALL(*engine_ptr_, ZoomUpdated).Times(0);
  TestUpdateGeometrySetsPluginRect(
      /*device_scale=*/2.0f, /*window_rect=*/gfx::Rect(2, 2, 0, 0),
      /*expected_device_scale=*/1.0f, /*expected_plugin_rect=*/gfx::Rect());
}

TEST_F(PdfViewWebPluginTest, UpdateGeometryScroll) {
  SetDocumentDimensions({100, 200});

  EXPECT_CALL(*client_ptr_, GetScrollPosition)
      .WillRepeatedly(Return(gfx::PointF(4.0f, 6.0f)));
  EXPECT_CALL(*engine_ptr_, ScrolledToXPosition(4));
  EXPECT_CALL(*engine_ptr_, ScrolledToYPosition(6));
  UpdatePluginGeometryWithoutWaiting(1.0f, gfx::Rect(3, 4, 5, 6));
}

TEST_F(PdfViewWebPluginTest, UpdateGeometryScrollStopped) {
  SetDocumentDimensions({100, 200});

  plugin_->OnMessage(ParseMessage(R"({
    "type": "stopScrolling",
  })"));

  EXPECT_CALL(*client_ptr_, GetScrollPosition)
      .WillRepeatedly(Return(gfx::PointF(4.0f, 6.0f)));
  EXPECT_CALL(*engine_ptr_, ScrolledToXPosition).Times(0);
  EXPECT_CALL(*engine_ptr_, ScrolledToYPosition).Times(0);
  UpdatePluginGeometryWithoutWaiting(1.0f, gfx::Rect(3, 4, 5, 6));
}

TEST_F(PdfViewWebPluginTest, UpdateGeometryScrollUnderflow) {
  SetDocumentDimensions({100, 200});

  EXPECT_CALL(*client_ptr_, GetScrollPosition)
      .WillRepeatedly(Return(gfx::PointF(-1.0f, -1.0f)));
  EXPECT_CALL(*engine_ptr_, ScrolledToXPosition(0));
  EXPECT_CALL(*engine_ptr_, ScrolledToYPosition(0));
  UpdatePluginGeometryWithoutWaiting(1.0f, gfx::Rect(3, 4, 5, 6));
}

TEST_F(PdfViewWebPluginTest, UpdateGeometryScrollOverflow) {
  SetDocumentDimensions({100, 200});

  EXPECT_CALL(*client_ptr_, GetScrollPosition)
      .WillRepeatedly(Return(gfx::PointF(96.0f, 195.0f)));
  EXPECT_CALL(*engine_ptr_, ScrolledToXPosition(95));
  EXPECT_CALL(*engine_ptr_, ScrolledToYPosition(194));
  UpdatePluginGeometryWithoutWaiting(1.0f, gfx::Rect(3, 4, 5, 6));
}

TEST_F(PdfViewWebPluginTest, UpdateGeometryScrollOverflowZoomed) {
  SetDocumentDimensions({100, 200});
  SendViewportMessage(/*zoom=*/2.0);

  EXPECT_CALL(*client_ptr_, GetScrollPosition)
      .WillRepeatedly(Return(gfx::PointF(196.0f, 395.0f)));
  EXPECT_CALL(*engine_ptr_, ScrolledToXPosition(195));
  EXPECT_CALL(*engine_ptr_, ScrolledToYPosition(394));
  UpdatePluginGeometryWithoutWaiting(1.0f, gfx::Rect(3, 4, 5, 6));
}

TEST_F(PdfViewWebPluginTest, UpdateGeometryScrollScaled) {
  SetDocumentDimensions({100, 200});

  EXPECT_CALL(*client_ptr_, GetScrollPosition)
      .WillRepeatedly(Return(gfx::PointF(4.0f, 6.0f)));
  EXPECT_CALL(*engine_ptr_, ScrolledToXPosition(4));
  EXPECT_CALL(*engine_ptr_, ScrolledToYPosition(6));
  UpdatePluginGeometryWithoutWaiting(2.0f, gfx::Rect(3, 4, 5, 6));
}

TEST_F(PdfViewWebPluginTest, UpdateGeometryScrollOverflowScaled) {
  SetDocumentDimensions({100, 200});

  EXPECT_CALL(*client_ptr_, GetScrollPosition)
      .WillRepeatedly(Return(gfx::PointF(195.0f, 395.0f)));
  EXPECT_CALL(*engine_ptr_, ScrolledToXPosition(194));
  EXPECT_CALL(*engine_ptr_, ScrolledToYPosition(394));
  UpdatePluginGeometryWithoutWaiting(2.0f, gfx::Rect(3, 4, 5, 6));
}

TEST_F(PdfViewWebPluginTest, SetCaretPosition) {
  SetDocumentDimensions({16, 9});
  UpdatePluginGeometryWithoutWaiting(1.0f, {10, 20, 20, 5});

  EXPECT_CALL(*engine_ptr_, SetCaretPosition(gfx::Point(2, 3)));
  plugin_->SetCaretPosition({4.0f, 3.0f});
}

TEST_F(PdfViewWebPluginTest, SetCaretPositionNegativeOrigin) {
  SetDocumentDimensions({16, 9});
  UpdatePluginGeometryWithoutWaiting(1.0f, {-10, -20, 20, 5});

  EXPECT_CALL(*engine_ptr_, SetCaretPosition(gfx::Point(2, 3)));
  plugin_->SetCaretPosition({4.0f, 3.0f});
}

TEST_F(PdfViewWebPluginTest, SetCaretPositionFractional) {
  SetDocumentDimensions({16, 9});
  UpdatePluginGeometryWithoutWaiting(1.0f, {10, 20, 20, 5});

  EXPECT_CALL(*engine_ptr_, SetCaretPosition(gfx::Point(1, 2)));
  plugin_->SetCaretPosition({3.9f, 2.9f});
}

TEST_F(PdfViewWebPluginTest, SetCaretPositionScaled) {
  SetDocumentDimensions({16, 9});
  UpdatePluginGeometryWithoutWaiting(2.0f, {20, 40, 40, 10});

  EXPECT_CALL(*engine_ptr_, SetCaretPosition(gfx::Point(4, 6)));
  plugin_->SetCaretPosition({4.0f, 3.0f});
}

TEST_F(PdfViewWebPluginTest, PaintEmptySnapshots) {
  TestPaintEmptySnapshots(/*device_scale=*/4.0f,
                          /*window_rect=*/gfx::Rect(10, 10, 20, 20),
                          /*paint_rect=*/gfx::Rect(5, 5, 15, 15),
                          /*expected_clipped_rect=*/gfx::Rect(10, 10, 10, 10));
}

TEST_F(PdfViewWebPluginTest, PaintSnapshots) {
  TestPaintSnapshots(/*device_scale=*/4.0f,
                     /*window_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*paint_rect=*/gfx::Rect(5, 5, 15, 15),
                     /*expected_clipped_rect=*/gfx::Rect(10, 10, 10, 10));
}

TEST_F(PdfViewWebPluginTest, PaintSnapshotsWithVariousDeviceScales) {
  static constexpr PaintParams kPaintWithVariousScalesParams[] = {
      {0.4f, gfx::Rect(8, 8, 30, 30), gfx::Rect(10, 10, 30, 30),
       gfx::Rect(10, 10, 28, 28)},
      {1.0f, gfx::Rect(8, 8, 30, 30), gfx::Rect(10, 10, 30, 30),
       gfx::Rect(10, 10, 28, 28)},
      {4.0f, gfx::Rect(8, 8, 30, 30), gfx::Rect(10, 10, 30, 30),
       gfx::Rect(10, 10, 28, 28)},
  };

  for (const auto& params : kPaintWithVariousScalesParams) {
    TestPaintSnapshots(params.device_scale, params.window_rect,
                       params.paint_rect, params.expected_clipped_rect);
  }
}

TEST_F(PdfViewWebPluginTest, PaintSnapshotsWithVariousRectPositions) {
  static constexpr PaintParams kPaintWithVariousPositionsParams[] = {
      // The window origin falls outside the `paint_rect` area.
      {4.0f, gfx::Rect(10, 10, 20, 20), gfx::Rect(5, 5, 15, 15),
       gfx::Rect(10, 10, 10, 10)},
      // The window origin falls within the `paint_rect` area.
      {4.0f, gfx::Rect(4, 4, 20, 20), gfx::Rect(8, 8, 15, 15),
       gfx::Rect(8, 8, 15, 15)},
  };

  for (const auto& params : kPaintWithVariousPositionsParams) {
    TestPaintSnapshots(params.device_scale, params.window_rect,
                       params.paint_rect, params.expected_clipped_rect);
  }
}

TEST_F(PdfViewWebPluginTest, OnPaintWithMultiplePaintRects) {
  SetDocumentDimensions({100, 200});
  UpdatePluginGeometryWithoutWaiting(/*device_scale=*/1.0f,
                                     gfx::Rect(0, 0, 40, 40));

  EXPECT_CALL(*engine_ptr_, Paint)
      .WillRepeatedly(
          [](const gfx::Rect& rect, SkBitmap& /*image_data*/,
             std::vector<gfx::Rect>& ready,
             std::vector<gfx::Rect>& /*pending*/) { ready.push_back(rect); });
  std::vector<PaintReadyRect> ready;
  std::vector<gfx::Rect> pending;
  plugin_->OnPaint(
      /*paint_rects=*/{gfx::Rect(5, 5, 10, 10), gfx::Rect(20, 20, 10, 10)},
      ready, pending);

  // Expect three paints: an initial background-clearing paint, and one for each
  // requested paint rectangle.
  ASSERT_THAT(ready, SizeIs(3));
  EXPECT_THAT(pending, IsEmpty());

  EXPECT_EQ(gfx::Rect(0, 0, 90, 90), ready[0].rect());
  EXPECT_TRUE(ready[0].flush_now());

  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), ready[1].rect());
  EXPECT_FALSE(ready[1].flush_now());

  EXPECT_EQ(gfx::Rect(20, 20, 10, 10), ready[2].rect());
  EXPECT_FALSE(ready[2].flush_now());

  // All the requested paints should share the same `SkImage`.
  EXPECT_NE(ready[0].image(), ready[1].image());
  EXPECT_EQ(ready[1].image(), ready[2].image());
}

TEST_F(PdfViewWebPluginTest, UpdateLayerTransformWithIdentity) {
  plugin_->UpdateLayerTransform(1.0f, gfx::Vector2dF());
  TestPaintSnapshots(/*device_scale=*/4.0f,
                     /*window_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*paint_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*expected_clipped_rect=*/gfx::Rect(10, 10, 20, 20));
}

TEST_F(PdfViewWebPluginTest, UpdateLayerTransformWithScale) {
  plugin_->UpdateLayerTransform(0.5f, gfx::Vector2dF());
  TestPaintSnapshots(/*device_scale=*/4.0f,
                     /*window_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*paint_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*expected_clipped_rect=*/gfx::Rect(10, 10, 10, 10));
}

TEST_F(PdfViewWebPluginTest, UpdateLayerTransformWithTranslate) {
  plugin_->UpdateLayerTransform(1.0f, gfx::Vector2dF(-1.25, 1.25));
  TestPaintSnapshots(/*device_scale=*/4.0f,
                     /*window_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*paint_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*expected_clipped_rect=*/gfx::Rect(10, 15, 15, 15));
}

TEST_F(PdfViewWebPluginTest, UpdateLayerTransformWithScaleAndTranslate) {
  plugin_->UpdateLayerTransform(0.5f, gfx::Vector2dF(-1.25, 1.25));
  TestPaintSnapshots(/*device_scale=*/4.0f,
                     /*window_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*paint_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*expected_clipped_rect=*/gfx::Rect(10, 15, 5, 10));
}

TEST_F(PdfViewWebPluginTest, HandleViewportMessageBeforeDocumentLoadComplete) {
  EXPECT_CALL(*engine_ptr_, ApplyDocumentLayout(DocumentLayout::Options()));
  EXPECT_CALL(*client_ptr_, PostMessage).Times(0);

  plugin_->OnMessage(ParseMessage(R"({
    "type": "viewport",
    "userInitiated": false,
    "zoom": 1,
    "layoutOptions": {
      "direction": 0,
      "defaultPageOrientation": 0,
      "twoUpViewEnabled": false,
    },
    "xOffset": 0,
    "yOffset": 0,
    "pinchPhase": 0,
  })"));
}

TEST_F(PdfViewWebPluginTest, HandleViewportMessageAfterDocumentLoadComplete) {
  plugin_->DocumentLoadComplete();

  EXPECT_CALL(*engine_ptr_, ApplyDocumentLayout(DocumentLayout::Options()));
  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(R"({
    "type": "loadProgress",
    "progress": 100.0,
  })")));

  plugin_->OnMessage(ParseMessage(R"({
    "type": "viewport",
    "userInitiated": false,
    "zoom": 1,
    "layoutOptions": {
      "direction": 0,
      "defaultPageOrientation": 0,
      "twoUpViewEnabled": false,
    },
    "xOffset": 0,
    "yOffset": 0,
    "pinchPhase": 0,
  })"));
}

TEST_F(PdfViewWebPluginTest, HandleViewportMessageSubsequently) {
  plugin_->OnMessage(ParseMessage(R"({
    "type": "viewport",
    "userInitiated": false,
    "zoom": 1,
    "layoutOptions": {
      "direction": 0,
      "defaultPageOrientation": 0,
      "twoUpViewEnabled": false,
    },
    "xOffset": 0,
    "yOffset": 0,
    "pinchPhase": 0,
  })"));

  DocumentLayout::Options two_up_options;
  two_up_options.set_page_spread(DocumentLayout::PageSpread::kTwoUpOdd);
  EXPECT_CALL(*engine_ptr_, ApplyDocumentLayout(two_up_options));
  EXPECT_CALL(*client_ptr_, PostMessage).Times(0);

  plugin_->OnMessage(ParseMessage(R"({
    "type": "viewport",
    "userInitiated": false,
    "zoom": 1,
    "layoutOptions": {
      "direction": 0,
      "defaultPageOrientation": 0,
      "twoUpViewEnabled": true,
    },
    "xOffset": 0,
    "yOffset": 0,
    "pinchPhase": 0,
  })"));
}

TEST_F(PdfViewWebPluginTest, HandleViewportMessageScroll) {
  EXPECT_CALL(*engine_ptr_, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  EXPECT_CALL(*engine_ptr_, ScrolledToXPosition(2));
  EXPECT_CALL(*engine_ptr_, ScrolledToYPosition(3));

  plugin_->OnMessage(ParseMessage(R"({
    "type": "viewport",
    "userInitiated": false,
    "zoom": 1,
    "layoutOptions": {
      "direction": 2,
      "defaultPageOrientation": 0,
      "twoUpViewEnabled": false,
    },
    "xOffset": 2,
    "yOffset": 3,
    "pinchPhase": 0,
  })"));
}

TEST_F(PdfViewWebPluginTest, HandleViewportMessageScrollRightToLeft) {
  EXPECT_CALL(*engine_ptr_, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  EXPECT_CALL(*engine_ptr_, ScrolledToXPosition(2));
  EXPECT_CALL(*engine_ptr_, ScrolledToYPosition(3));

  plugin_->OnMessage(ParseMessage(R"({
    "type": "viewport",
    "userInitiated": false,
    "zoom": 1,
    "layoutOptions": {
      "direction": 1,
      "defaultPageOrientation": 0,
      "twoUpViewEnabled": false,
    },
    "xOffset": 2,
    "yOffset": 3,
    "pinchPhase": 0,
  })"));
}

TEST_F(PdfViewWebPluginTest, HandleSetBackgroundColorMessage) {
  ASSERT_NE(SK_ColorGREEN, plugin_->GetBackgroundColor());

  plugin_->OnMessage(base::Value::Dict()
                         .Set("type", "setBackgroundColor")
                         .Set("color", static_cast<double>(SK_ColorGREEN)));

  EXPECT_EQ(SK_ColorGREEN, plugin_->GetBackgroundColor());
}

TEST_F(PdfViewWebPluginTest, HandleSetPresentationModeMessage) {
  EXPECT_FALSE(engine_ptr_->IsReadOnly());
  plugin_->set_cursor_type_for_testing(ui::mojom::CursorType::kIBeam);

  auto message = base::Value::Dict()
                     .Set("type", "setPresentationMode")
                     .Set("enablePresentationMode", true);
  plugin_->OnMessage(message);

  // After entering presentation mode, PDFiumEngine is read-only and the cursor
  // type has been reset to a pointer.
  EXPECT_TRUE(engine_ptr_->IsReadOnly());
  EXPECT_EQ(ui::mojom::CursorType::kPointer,
            plugin_->cursor_for_testing().type());

  message.Set("enablePresentationMode", false);
  plugin_->OnMessage(message);

  // After exiting presentation mode, PDFiumEngine is no longer read-only.
  // The cursor remains as a pointer until the next input event updates it.
  EXPECT_FALSE(engine_ptr_->IsReadOnly());
  EXPECT_EQ(ui::mojom::CursorType::kPointer,
            plugin_->cursor_for_testing().type());
}

TEST_F(PdfViewWebPluginTest, HandleInputEvent) {
  UpdatePluginGeometryWithoutWaiting(2.0f, {0, 0, 20, 20});

  EXPECT_CALL(*engine_ptr_, HandleInputEvent)
      .WillRepeatedly([](const blink::WebInputEvent& event) {
        if (!blink::WebInputEvent::IsMouseEventType(event.GetType())) {
          ADD_FAILURE() << "Unexpected event type: " << event.GetType();
          return false;
        }

        const auto& mouse_event =
            static_cast<const blink::WebMouseEvent&>(event);
        EXPECT_EQ(blink::WebInputEvent::Type::kMouseDown,
                  mouse_event.GetType());
        EXPECT_EQ(gfx::PointF(10.0f, 40.0f), mouse_event.PositionInWidget());
        return true;
      });

  blink::WebMouseEvent mouse_event;
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseDown);
  mouse_event.SetPositionInWidget(10.0f, 20.0f);

  TestSendInputEvent(mouse_event,
                     blink::WebInputEventResult::kHandledApplication);
}

class PdfViewWebPluginImeTest : public PdfViewWebPluginTest {
 public:
  void TestImeSetCompositionForPlugin(const blink::WebString& text) {
    EXPECT_CALL(*engine_ptr_, HandleInputEvent).Times(0);
    plugin_->ImeSetCompositionForPlugin(text, std::vector<ui::ImeTextSpan>(),
                                        gfx::Range(),
                                        /*selection_start=*/0,
                                        /*selection_end=*/0);
  }

  void TestImeFinishComposingTextForPlugin(
      const blink::WebString& expected_text) {
    InSequence sequence;
    std::u16string expected_text16 = expected_text.Utf16();
    if (expected_text16.size()) {
      for (const auto& c : expected_text16) {
        std::u16string_view expected_key(&c, 1);
        EXPECT_CALL(*engine_ptr_,
                    HandleInputEvent(IsExpectedImeKeyEvent(expected_key)))
            .WillOnce(Return(true));
      }
    } else {
      EXPECT_CALL(*engine_ptr_, HandleInputEvent).Times(0);
    }
    plugin_->ImeFinishComposingTextForPlugin(false);
  }

  void TestImeCommitTextForPlugin(const blink::WebString& text) {
    InSequence sequence;
    std::u16string expected_text16 = text.Utf16();
    if (expected_text16.size()) {
      for (const auto& c : expected_text16) {
        std::u16string_view event(&c, 1);
        EXPECT_CALL(*engine_ptr_,
                    HandleInputEvent(IsExpectedImeKeyEvent(event)))
            .WillOnce(Return(true));
      }
    } else {
      EXPECT_CALL(*engine_ptr_, HandleInputEvent).Times(0);
    }
    plugin_->ImeCommitTextForPlugin(text, std::vector<ui::ImeTextSpan>(),
                                    gfx::Range(),
                                    /*relative_cursor_pos=*/0);
  }
};

TEST_F(PdfViewWebPluginImeTest, ImeSetCompositionAndFinishAscii) {
  const blink::WebString text = blink::WebString::FromASCII("input");
  TestImeSetCompositionForPlugin(text);
  TestImeFinishComposingTextForPlugin(text);
}

TEST_F(PdfViewWebPluginImeTest, ImeSetCompositionAndFinishUnicode) {
  const blink::WebString text = blink::WebString::FromUTF16(u"你好");
  TestImeSetCompositionForPlugin(text);
  TestImeFinishComposingTextForPlugin(text);
  // Calling ImeFinishComposingTextForPlugin() again is a no-op.
  TestImeFinishComposingTextForPlugin("");
}

TEST_F(PdfViewWebPluginImeTest, ImeSetCompositionAndFinishEmpty) {
  const blink::WebString text;
  TestImeSetCompositionForPlugin(text);
  TestImeFinishComposingTextForPlugin(text);
}

TEST_F(PdfViewWebPluginImeTest, ImeCommitTextForPluginAscii) {
  const blink::WebString text = blink::WebString::FromASCII("a b");
  TestImeCommitTextForPlugin(text);
}

TEST_F(PdfViewWebPluginImeTest, ImeCommitTextForPluginUnicode) {
  const blink::WebString text = blink::WebString::FromUTF16(u"さようなら");
  TestImeCommitTextForPlugin(text);
}

TEST_F(PdfViewWebPluginImeTest, ImeCommitTextForPluginEmpty) {
  const blink::WebString text;
  TestImeCommitTextForPlugin(text);
}

TEST_F(PdfViewWebPluginTest, SelectionChanged) {
  plugin_->EnableAccessibility();
  plugin_->DocumentLoadComplete();
  UpdatePluginGeometryWithoutWaiting(1.0f, {300, 56, 20, 5});
  SetDocumentDimensions({16, 9});

  AccessibilityViewportInfo viewport_info;
  EXPECT_CALL(pdf_host_, SelectionChanged(gfx::PointF(-8.0f, -20.0f), 40,
                                          gfx::PointF(52.0f, 60.0f), 80));
  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityViewportInfo)
      .WillOnce(SaveArg<0>(&viewport_info));
  plugin_->SelectionChanged({-10, -20, 30, 40}, {50, 60, 70, 80});

  EXPECT_EQ(gfx::Point(), viewport_info.scroll);
  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginTest, SelectionChangedNegativeOrigin) {
  plugin_->EnableAccessibility();
  plugin_->DocumentLoadComplete();
  UpdatePluginGeometryWithoutWaiting(1.0f, {-300, -56, 20, 5});
  SetDocumentDimensions({16, 9});

  AccessibilityViewportInfo viewport_info;
  EXPECT_CALL(pdf_host_, SelectionChanged(gfx::PointF(-8.0f, -20.0f), 40,
                                          gfx::PointF(52.0f, 60.0f), 80));
  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityViewportInfo)
      .WillOnce(SaveArg<0>(&viewport_info));
  plugin_->SelectionChanged({-10, -20, 30, 40}, {50, 60, 70, 80});

  EXPECT_EQ(gfx::Point(), viewport_info.scroll);
  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginTest, SelectionChangedScaled) {
  plugin_->EnableAccessibility();
  plugin_->DocumentLoadComplete();
  UpdatePluginGeometryWithoutWaiting(2.0f, {600, 112, 40, 10});
  SetDocumentDimensions({16, 9});

  AccessibilityViewportInfo viewport_info;
  EXPECT_CALL(pdf_host_, SelectionChanged(gfx::PointF(-8.0f, -20.0f), 40,
                                          gfx::PointF(52.0f, 60.0f), 80));
  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityViewportInfo)
      .WillOnce(SaveArg<0>(&viewport_info));
  plugin_->SelectionChanged({-20, -40, 60, 80}, {100, 120, 140, 160});

  EXPECT_EQ(gfx::Point(), viewport_info.scroll);
  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginTest, ChangeTextSelection) {
  ASSERT_FALSE(plugin_->HasSelection());
  ASSERT_TRUE(plugin_->SelectionAsText().IsEmpty());
  ASSERT_TRUE(plugin_->SelectionAsMarkup().IsEmpty());

  static constexpr char kSelectedText[] = "1234";
  EXPECT_CALL(*client_ptr_,
              TextSelectionChanged(blink::WebString::FromUTF8(kSelectedText), 0,
                                   gfx::Range(0, 4)));

  plugin_->SetSelectedText(kSelectedText);
  EXPECT_TRUE(plugin_->HasSelection());
  EXPECT_EQ(kSelectedText, plugin_->SelectionAsText().Utf8());
  EXPECT_EQ(kSelectedText, plugin_->SelectionAsMarkup().Utf8());

  static constexpr char kEmptyText[] = "";
  EXPECT_CALL(*client_ptr_,
              TextSelectionChanged(blink::WebString::FromUTF8(kEmptyText), 0,
                                   gfx::Range(0, 0)));
  plugin_->SetSelectedText(kEmptyText);
  EXPECT_FALSE(plugin_->HasSelection());
  EXPECT_TRUE(plugin_->SelectionAsText().IsEmpty());
  EXPECT_TRUE(plugin_->SelectionAsMarkup().IsEmpty());
}

TEST_F(PdfViewWebPluginTest, SelectAll) {
  EXPECT_CALL(*engine_ptr_, SelectAll);

  EXPECT_TRUE(plugin_->ExecuteEditCommand(
      /*name=*/blink::WebString::FromASCII("SelectAll"),
      /*value=*/blink::WebString()));
}

TEST_F(PdfViewWebPluginTest, FormTextFieldFocusChangeUpdatesTextInputType) {
  ASSERT_EQ(blink::WebTextInputType::kWebTextInputTypeNone,
            plugin_->GetPluginTextInputType());

  ExpectUpdateTextInputState(blink::WebTextInputType::kWebTextInputTypeText);
  plugin_->FormFieldFocusChange(PDFiumEngineClient::FocusFieldType::kText);

  ExpectUpdateTextInputState(blink::WebTextInputType::kWebTextInputTypeNone);
  plugin_->FormFieldFocusChange(PDFiumEngineClient::FocusFieldType::kNoFocus);

  ExpectUpdateTextInputState(blink::WebTextInputType::kWebTextInputTypeText);
  plugin_->FormFieldFocusChange(PDFiumEngineClient::FocusFieldType::kText);

  ExpectUpdateTextInputState(blink::WebTextInputType::kWebTextInputTypeNone);
  plugin_->FormFieldFocusChange(PDFiumEngineClient::FocusFieldType::kNonText);
}

TEST_F(PdfViewWebPluginTest, SearchString) {
  static constexpr char16_t kNeedle[] = u"fox";
  static constexpr char16_t kHaystack[] =
      u"The quick brown fox jumped over the lazy Fox";

  {
    static constexpr PDFiumEngineClient::SearchStringResult kExpectation[] = {
        {16, 3}};
    EXPECT_THAT(
        plugin_->SearchString(kNeedle, kHaystack, /*case_sensitive=*/true),
        Pointwise(SearchStringResultEq(), kExpectation));
  }
  {
    static constexpr PDFiumEngineClient::SearchStringResult kExpectation[] = {
        {16, 3}, {41, 3}};
    EXPECT_THAT(
        plugin_->SearchString(kNeedle, kHaystack, /*case_sensitive=*/false),
        Pointwise(SearchStringResultEq(), kExpectation));
  }
}

TEST_F(PdfViewWebPluginTest, UpdateFocus) {
  MockFunction<void(int checkpoint_num)> checkpoint;

  {
    InSequence sequence;

    // Focus false -> true: Triggers updates.
    EXPECT_CALL(*client_ptr_, UpdateTextInputState);
    EXPECT_CALL(*client_ptr_, UpdateSelectionBounds);
    EXPECT_CALL(checkpoint, Call(1));

    // Focus true -> true: No updates.
    EXPECT_CALL(checkpoint, Call(2));

    // Focus true -> false: Triggers updates.
    EXPECT_CALL(*client_ptr_, UpdateTextInputState);
    EXPECT_CALL(*client_ptr_, UpdateSelectionBounds);
    EXPECT_CALL(checkpoint, Call(3));

    // Focus false -> false: No updates.
    EXPECT_CALL(checkpoint, Call(4));

    // Focus false -> true: Triggers updates.
    EXPECT_CALL(*client_ptr_, UpdateTextInputState);
    EXPECT_CALL(*client_ptr_, UpdateSelectionBounds);
  }

  // The focus type does not matter in this test.
  plugin_->UpdateFocus(/*focused=*/true, blink::mojom::FocusType::kNone);
  checkpoint.Call(1);
  plugin_->UpdateFocus(/*focused=*/true, blink::mojom::FocusType::kNone);
  checkpoint.Call(2);
  plugin_->UpdateFocus(/*focused=*/false, blink::mojom::FocusType::kNone);
  checkpoint.Call(3);
  plugin_->UpdateFocus(/*focused=*/false, blink::mojom::FocusType::kNone);
  checkpoint.Call(4);
  plugin_->UpdateFocus(/*focused=*/true, blink::mojom::FocusType::kNone);
}

TEST_F(PdfViewWebPluginTest, ShouldDispatchImeEventsToPlugin) {
  ASSERT_TRUE(plugin_->ShouldDispatchImeEventsToPlugin());
}

TEST_F(PdfViewWebPluginTest, CaretChange) {
  EXPECT_CALL(*engine_ptr_, ZoomUpdated(2.0f));
  UpdatePluginGeometry(
      /*device_scale=*/2.0f, /*window_rect=*/gfx::Rect(12, 24, 36, 48));
  plugin_->CaretChanged(gfx::Rect(10, 20, 30, 40));
  EXPECT_EQ(gfx::Rect(28, 20, 30, 40), plugin_->GetPluginCaretBounds());
}

TEST_F(PdfViewWebPluginTest, EnteredEditMode) {
  EXPECT_CALL(pdf_host_, SetPluginCanSave(true));
  EXPECT_CALL(*client_ptr_, PostMessage).Times(AnyNumber());
  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(R"({
    "type": "setIsEditing",
  })")));
  plugin_->EnteredEditMode();

  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginTest, NotifyNumberOfFindResultsChanged) {
  plugin_->StartFind("x", /*case_sensitive=*/false, /*identifier=*/123);

  const std::vector<gfx::Rect> tickmarks = {gfx::Rect(1, 2), gfx::Rect(3, 4)};
  plugin_->UpdateTickMarks(tickmarks);

  EXPECT_CALL(*client_ptr_, ReportFindInPageTickmarks(tickmarks));
  EXPECT_CALL(*client_ptr_, ReportFindInPageMatchCount(123, 5, true));
  plugin_->NotifyNumberOfFindResultsChanged(/*total=*/5, /*final_result=*/true);
}

TEST_F(PdfViewWebPluginTest, OnDocumentLoadComplete) {
  auto message =
      base::Value::Dict()
          .Set("type", "metadata")
          .Set("metadataData", base::Value::Dict()
                                   .Set("fileSize", "0 B")
                                   .Set("linearized", false)
                                   .Set("pageSize", "Varies")
                                   .Set("canSerializeDocument", true));

  EXPECT_CALL(*client_ptr_, PostMessage);
  EXPECT_CALL(*client_ptr_, PostMessage(Eq(std::ref(message))));
  plugin_->DocumentLoadComplete();
}

TEST_F(PdfViewWebPluginTest, OnRendererPreferencesUpdated) {
  auto message = base::Value::Dict()
                     .Set("type", "rendererPreferencesUpdated")
                     .Set("caretBrowsingEnabled", false);

  InSequence seq;

  EXPECT_CALL(*client_ptr_, PostMessage(Eq(std::ref(message))));
  EXPECT_CALL(*engine_ptr_, SetCaretBrowsingEnabled(false));
  EXPECT_CALL(*engine_ptr_,
              SetCaretBlinkInterval(PdfCaret::kDefaultBlinkInterval));

  blink::RendererPreferences prefs;
  plugin_->OnRendererPreferencesUpdated(prefs);

  message.Set("caretBrowsingEnabled", true);

  EXPECT_CALL(*client_ptr_, PostMessage(Eq(std::ref(message))));
  EXPECT_CALL(*engine_ptr_, SetCaretBrowsingEnabled(true));
  constexpr base::TimeDelta kBlinkInterval = base::Milliseconds(300);
  EXPECT_CALL(*engine_ptr_, SetCaretBlinkInterval(kBlinkInterval));

  prefs.caret_browsing_enabled = true;
  prefs.caret_blink_interval = kBlinkInterval;
  plugin_->OnRendererPreferencesUpdated(prefs);

  EXPECT_CALL(*client_ptr_, PostMessage(Eq(std::ref(message))));
  EXPECT_CALL(*engine_ptr_, SetCaretBrowsingEnabled(true));
  EXPECT_CALL(*engine_ptr_, SetCaretBlinkInterval(base::TimeDelta()));

  prefs.caret_blink_interval = base::TimeDelta();
  plugin_->OnRendererPreferencesUpdated(prefs);
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
// Searchify in progress not shown when searchify just starts.
TEST_F(PdfViewWebPluginTest, OnSearchifyStarted) {
  base::Value::Dict message = GenerateShowSearchifyInProgressMessage(true);

  EXPECT_CALL(*client_ptr_, PostMessage(Eq(std::ref(message)))).Times(0);
  EXPECT_CALL(pdf_host_, OnSearchifyStarted);

  plugin_->OnSearchifyStateChange(true);

  pdf_receiver_.FlushForTesting();
}

// Searchify in progress not shown when searchify didn't starts.
TEST_F(PdfViewWebPluginTest, OnSearchifyNotStartedAndMaybeShowInProgress) {
  base::Value::Dict message = GenerateShowSearchifyInProgressMessage(true);

  EXPECT_CALL(*client_ptr_, PostMessage(Eq(std::ref(message)))).Times(0);
  EXPECT_CALL(pdf_host_, OnSearchifyStarted).Times(0);

  plugin_->MaybeShowSearchifyInProgress();

  pdf_receiver_.FlushForTesting();
}

// Searchify in progress shown when asked after start.
TEST_F(PdfViewWebPluginTest, OnSearchifyStartedAndMaybeShowInProgress) {
  base::Value::Dict message = GenerateShowSearchifyInProgressMessage(true);

  EXPECT_CALL(*client_ptr_, PostMessage(Eq(std::ref(message))));
  EXPECT_CALL(pdf_host_, OnSearchifyStarted);

  plugin_->OnSearchifyStateChange(true);
  plugin_->MaybeShowSearchifyInProgress();

  pdf_receiver_.FlushForTesting();
}

// Searchify in progress not shown when asked after stop.
TEST_F(PdfViewWebPluginTest,
       OnSearchifyStartedAndStoppedAndMaybeShowInProgress) {
  base::Value::Dict message_show = GenerateShowSearchifyInProgressMessage(true);

  EXPECT_CALL(*client_ptr_, PostMessage(Eq(std::ref(message_show)))).Times(0);
  EXPECT_CALL(pdf_host_, OnSearchifyStarted);

  plugin_->OnSearchifyStateChange(true);
  plugin_->OnSearchifyStateChange(false);
  plugin_->MaybeShowSearchifyInProgress();

  pdf_receiver_.FlushForTesting();
}

// Searchify in progress hides after stop.
TEST_F(PdfViewWebPluginTest, OnSearchifyShowProgressHideAfterStopped) {
  base::Value::Dict message_show = GenerateShowSearchifyInProgressMessage(true);
  base::Value::Dict message_hide =
      GenerateShowSearchifyInProgressMessage(false);

  EXPECT_CALL(*client_ptr_, PostMessage(Eq(std::ref(message_show))));
  EXPECT_CALL(*client_ptr_, PostMessage(Eq(std::ref(message_hide))));

  plugin_->OnSearchifyStateChange(true);
  plugin_->MaybeShowSearchifyInProgress();
  plugin_->OnSearchifyStateChange(false);

  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginTest, OnSearchifyStartedMoreThanOnce) {
  plugin_->OnSearchifyStateChange(true);
  plugin_->OnSearchifyStateChange(false);
  plugin_->OnSearchifyStateChange(true);

  EXPECT_CALL(pdf_host_, OnSearchifyStarted);

  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginTest, OnHasSearchifyText) {
  auto message = base::Value::Dict().Set("type", "setHasSearchifyText");

  EXPECT_CALL(*client_ptr_, PostMessage(Eq(std::ref(message))));
  plugin_->OnHasSearchifyText();
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

TEST_F(PdfViewWebPluginTest, FindAndHighlightTextFragments) {
  EXPECT_CALL(*engine_ptr_, FindAndHighlightTextFragments(
                                ElementsAre("hello-,world", "world,-hello")))
      .WillOnce(Return(true));
  EXPECT_CALL(*engine_ptr_, ScrollToFirstTextFragment);

  plugin_->OnMessage(base::Value::Dict()
                         .Set("type", "highlightTextFragments")
                         .Set("textFragments", base::Value::List()
                                                   .Append("hello-,world")
                                                   .Append("world,-hello")));
}

TEST_F(PdfViewWebPluginTest, FindAndHighlightTextFragmentsNotFound) {
  EXPECT_CALL(*engine_ptr_, FindAndHighlightTextFragments(
                                ElementsAre("hello-,world", "world,-hello")))
      .WillOnce(Return(false));
  EXPECT_CALL(*engine_ptr_, ScrollToFirstTextFragment).Times(0);

  plugin_->OnMessage(base::Value::Dict()
                         .Set("type", "highlightTextFragments")
                         .Set("textFragments", base::Value::List()
                                                   .Append("hello-,world")
                                                   .Append("world,-hello")));
}

class PdfViewWebPluginWithDocInfoTest
    : public PdfViewWebPluginTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    PdfViewWebPluginTest::SetUp();
    if (IsPortfolioEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(features::kPdfPortfolio);
    }
  }

  bool IsPortfolioEnabled() { return GetParam(); }

 protected:
  class TestPDFiumEngineWithDocInfo : public TestPDFiumEngine {
   public:
    explicit TestPDFiumEngineWithDocInfo(PDFiumEngineClient* client)
        : TestPDFiumEngine(client) {
      InitializeDocumentAttachments();
      InitializeDocumentMetadata();
    }

    base::Value::List GetBookmarks() override {
      // Create `bookmark2` which navigates to a web page. This bookmark will be
      // a child of `bookmark1`.
      auto bookmark2 =
          base::Value::Dict().Set("title", "Bookmark 2").Set("uri", "test.com");

      // Create `bookmark1` which navigates to an in-doc position. This bookmark
      // will be in the top-level bookmark list.
      auto bookmark1 =
          base::Value::Dict()
              .Set("title", "Bookmark 1")
              .Set("page", 2)
              .Set("x", 10)
              .Set("y", 20)
              .Set("zoom", 2.0)
              .Set("children",
                   base::Value::List().Append(std::move(bookmark2)));

      // Create the top-level bookmark list.
      return base::Value::List().Append(std::move(bookmark1));
    }

    std::optional<gfx::Size> GetUniformPageSizePoints() override {
      return gfx::Size(1000, 1200);
    }

   private:
    void InitializeDocumentAttachments() {
      doc_attachment_info_list().resize(3);

      // A regular attachment.
      doc_attachment_info_list()[0].name = u"attachment1.txt";
      doc_attachment_info_list()[0].creation_date = u"D:20170712214438-07'00'";
      doc_attachment_info_list()[0].modified_date = u"D:20160115091400";
      doc_attachment_info_list()[0].is_readable = true;
      doc_attachment_info_list()[0].size_bytes = 13u;

      // An unreadable attachment.
      doc_attachment_info_list()[1].name = u"attachment2.pdf";
      doc_attachment_info_list()[1].is_readable = false;

      // A readable attachment that exceeds download size limit.
      doc_attachment_info_list()[2].name = u"attachment3.mov";
      doc_attachment_info_list()[2].is_readable = true;
      doc_attachment_info_list()[2].size_bytes =
          PdfViewWebPlugin::kMaximumSavedFileSize + 1;
    }

    void InitializeDocumentMetadata() {
      metadata().version = PdfVersion::k1_7;
      metadata().size_bytes = 13u;
      metadata().page_count = 13u;
      metadata().linearized = true;
      metadata().has_attachments = true;
      metadata().form_type = FormType::kAcroForm;
      metadata().title = "Title";
      metadata().author = "Author";
      metadata().subject = "Subject";
      metadata().keywords = "Keywords";
      metadata().creator = "Creator";
      metadata().producer = "Producer";
      ASSERT_TRUE(base::Time::FromUTCString("2021-05-04 11:12:13",
                                            &metadata().creation_date));
      ASSERT_TRUE(base::Time::FromUTCString("2021-06-04 15:16:17",
                                            &metadata().mod_date));
    }

    // Note: In the metadata message `creation_date` and `mod_date` are
    // locale-specific strings for display to the user. So this test uses a
    // specific locale.
    base::test::ScopedRestoreICUDefaultLocale restore_locale_{"en_US"};
    base::test::ScopedRestoreDefaultTimezone restore_timezone_{
        "America/Los_Angeles"};
  };

  static base::Value::Dict CreateExpectedAttachmentsResponse() {
    return base::Value::Dict()
        .Set("type", "attachments")
        .Set("attachmentsData", base::Value::List()
                                    .Append(base::Value::Dict()
                                                .Set("name", "attachment1.txt")
                                                .Set("size", 13)
                                                .Set("readable", true))
                                    .Append(base::Value::Dict()
                                                .Set("name", "attachment2.pdf")
                                                .Set("size", 0)
                                                .Set("readable", false))
                                    .Append(base::Value::Dict()
                                                .Set("name", "attachment3.mov")
                                                .Set("size", -1)
                                                .Set("readable", true)));
  }

  static base::Value::Dict CreateExpectedBookmarksResponse(
      base::Value::List bookmarks) {
    return base::Value::Dict()
        .Set("type", "bookmarks")
        .Set("bookmarksData", std::move(bookmarks));
  }

  static base::Value::Dict CreateExpectedMetadataResponse() {
    return base::Value::Dict()
        .Set("type", "metadata")
        .Set("metadataData", base::Value::Dict()
                                 .Set("version", "1.7")
                                 .Set("fileSize", "13 B")
                                 .Set("linearized", true)
                                 .Set("title", "Title")
                                 .Set("author", "Author")
                                 .Set("subject", "Subject")
                                 .Set("keywords", "Keywords")
                                 .Set("creator", "Creator")
                                 .Set("producer", "Producer")
                                 .Set("creationDate",
                                      "5/4/21, 4:12:13\xE2\x80\xAF"
                                      "AM")
                                 .Set("modDate",
                                      "6/4/21, 8:16:17\xE2\x80\xAF"
                                      "AM")
                                 .Set("pageSize", "13.89 × 16.67 in (portrait)")
                                 .Set("canSerializeDocument", true));
  }

  void SetUpClient() override {
    EXPECT_CALL(*client_ptr_, CreateEngine).WillOnce([this]() {
      auto engine = std::make_unique<NiceMock<TestPDFiumEngineWithDocInfo>>(
          plugin_.get());
      engine_ptr_ = engine.get();
      return engine;
    });
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(PdfViewWebPluginWithDocInfoTest, OnDocumentLoadComplete) {
  const base::Value::Dict expect_attachments =
      CreateExpectedAttachmentsResponse();
  const base::Value::Dict expect_bookmarks =
      CreateExpectedBookmarksResponse(engine_ptr_->GetBookmarks());
  const base::Value::Dict expect_metadata = CreateExpectedMetadataResponse();
  EXPECT_CALL(*client_ptr_, PostMessage);
  EXPECT_CALL(*client_ptr_, PostMessage(Eq(std::ref(expect_attachments))))
      .Times(IsPortfolioEnabled() ? 1 : 0);
  EXPECT_CALL(*client_ptr_, PostMessage(Eq(std::ref(expect_bookmarks))));
  EXPECT_CALL(*client_ptr_, PostMessage(Eq(std::ref(expect_metadata))));
  plugin_->DocumentLoadComplete();
}

INSTANTIATE_TEST_SUITE_P(All, PdfViewWebPluginWithDocInfoTest, testing::Bool());

class PdfViewWebPluginSaveTest : public PdfViewWebPluginTest {
 protected:
  static void AddDataToValue(base::span<const uint8_t> data,
                             base::Value& value) {
    value.GetDict().Set("dataToSave", base::Value(data));
  }

  void SetUpClient() override {
    // Ignore non-"saveData" `PdfViewWebPlugin::Client::PostMessage()` calls.
    EXPECT_CALL(*client_ptr_, PostMessage)
        .WillRepeatedly([](const base::Value::Dict& message) {
          EXPECT_NE("saveData", *message.FindString("type"));
        });
  }
};

TEST_F(PdfViewWebPluginSaveTest, OriginalInNonEditMode) {
  {
    InSequence pdf_host_sequence;

    EXPECT_CALL(pdf_host_, SaveUrlAs(GURL(kPdfUrl),
                                     network::mojom::ReferrerPolicy::kDefault));
  }

  EXPECT_EQ(blink::WebTextInputType::kWebTextInputTypeNone,
            plugin_->GetPluginTextInputType());

  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(R"({
    "type": "consumeSaveToken",
    "token": "original-in-non-edit-mode",
  })")));

  plugin_->OnMessage(ParseMessage(R"({
    "type": "save",
    "saveRequestType": "ORIGINAL",
    "token": "original-in-non-edit-mode",
  })"));

  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginSaveTest, OriginalInEditMode) {
  plugin_->EnteredEditMode();
  pdf_receiver_.FlushForTesting();

  {
    InSequence pdf_host_sequence;

    EXPECT_CALL(pdf_host_, SetPluginCanSave(false));
    EXPECT_CALL(pdf_host_, SaveUrlAs(GURL(kPdfUrl),
                                     network::mojom::ReferrerPolicy::kDefault));
    EXPECT_CALL(pdf_host_, SetPluginCanSave(true));
  }

  EXPECT_EQ(blink::WebTextInputType::kWebTextInputTypeNone,
            plugin_->GetPluginTextInputType());

  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(R"({
    "type": "consumeSaveToken",
    "token": "original-in-edit-mode",
  })")));

  plugin_->OnMessage(ParseMessage(R"({
    "type": "save",
    "saveRequestType": "ORIGINAL",
    "token": "original-in-edit-mode",
  })"));

  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginSaveTest, EditedInEditMode) {
  plugin_->EnteredEditMode();

  EXPECT_EQ(blink::WebTextInputType::kWebTextInputTypeNone,
            plugin_->GetPluginTextInputType());

  base::Value expected_response = base::test::ParseJson(R"({
    "type": "saveData",
    "token": "edited-in-edit-mode",
    "fileName": "example.pdf",
    "editModeForTesting": true,
  })");
  AddDataToValue(base::span(TestPDFiumEngine::kSaveData), expected_response);
  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(expected_response)));

  plugin_->OnMessage(ParseMessage(R"({
    "type": "save",
    "saveRequestType": "EDITED",
    "token": "edited-in-edit-mode",
  })"));
}

class PdfViewWebPluginSaveInBlocksTest : public PdfViewWebPluginTest {
 protected:
  base::Value::Dict CreateRequest(pdf::mojom::SaveRequestType request_type,
                                  uint32_t offset,
                                  uint32_t block_size,
                                  std::string token) {
    std::string request_type_string;
    switch (request_type) {
      case pdf::mojom::SaveRequestType::kAnnotation:
        request_type_string = "ANNOTATION";
        break;
      case pdf::mojom::SaveRequestType::kOriginal:
        request_type_string = "ORIGINAL";
        break;
      case pdf::mojom::SaveRequestType::kEdited:
        request_type_string = "EDITED";
        break;
      case pdf::mojom::SaveRequestType::kSearchified:
        request_type_string = "SEARCHIFIED";
        break;
    }
    return base::Value::Dict()
        .Set("type", "getSaveDataBlock")
        .Set("saveRequestType", request_type_string)
        .Set("offset", static_cast<int>(offset))
        .Set("blockSize", static_cast<int>(block_size))
        .Set("token", token);
  }

  void ExpectResponse(base::span<const uint8_t> data,
                      uint32_t offset,
                      uint32_t block_size,
                      std::string token) {
    auto data_to_save = data.subspan(offset, block_size);
    base::BlobStorage data_to_save_blob(data_to_save.begin(),
                                        data_to_save.end());
    auto dict = base::Value::Dict()
                    .Set("type", "saveDataBlock")
                    .Set("token", std::move(token))
                    .Set("dataToSave", std::move(data_to_save_blob))
                    .Set("totalFileSize", static_cast<int>(data.size()));
    EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(dict)));
  }

  void SetUpClient() override {
    // Ignore non - "saveDataBlock" `PdfViewWebPlugin::Client::PostMessage()`
    // calls.
    EXPECT_CALL(*client_ptr_, PostMessage)
        .WillRepeatedly([](const base::Value::Dict& message) {
          EXPECT_NE("saveDataBlock", *message.FindString("type"));
        });
  }
};

TEST_F(PdfViewWebPluginSaveInBlocksTest, GetSuggestedFileName) {
  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(R"({
    "type": "getSuggestedFileNameReply",
    "messageId": "foo",
    "fileName": "example.pdf",
  })")));

  plugin_->OnMessage(ParseMessage(R"({
    "type": "getSuggestedFileName",
    "messageId": "foo",
  })"));

  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginSaveInBlocksTest, OriginalInOneBlock) {
  base::span data(TestPDFiumEngine::kLoadedData);
  ExpectResponse(data, 0, data.size(), "token-1");
  plugin_->OnMessage(
      CreateRequest(pdf::mojom::SaveRequestType::kOriginal, 0, 0, "token-1"));
  EXPECT_TRUE(plugin_->IsSaveDataBufferEmptyForTesting());
  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginSaveInBlocksTest, OriginalInMulipleBlocks) {
  plugin_->SetMaxSaveBufferSizeForTesting(3);

  base::span data(TestPDFiumEngine::kLoadedData);
  ASSERT_GT(data.size(), 3u);
  ExpectResponse(data, 0, 3, "token-1");
  ExpectResponse(data, 3, data.size() - 3, "token-2");
  plugin_->OnMessage(
      CreateRequest(pdf::mojom::SaveRequestType::kOriginal, 0, 0, "token-1"));
  EXPECT_TRUE(plugin_->IsSaveDataBufferEmptyForTesting());
  plugin_->OnMessage(CreateRequest(pdf::mojom::SaveRequestType::kOriginal, 3,
                                   data.size() - 3, "token-2"));
  EXPECT_TRUE(plugin_->IsSaveDataBufferEmptyForTesting());
  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginSaveInBlocksTest, EditedInOneBlock) {
  plugin_->EnteredEditMode();

  base::span data(TestPDFiumEngine::kSaveData);
  ExpectResponse(data, 0, data.size(), "token-1");
  plugin_->OnMessage(
      CreateRequest(pdf::mojom::SaveRequestType::kEdited, 0, 0, "token-1"));
  EXPECT_TRUE(plugin_->IsSaveDataBufferEmptyForTesting());
  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginSaveInBlocksTest, EditedInMultipleBlock) {
  plugin_->EnteredEditMode();
  plugin_->SetMaxSaveBufferSizeForTesting(2);

  base::span data(TestPDFiumEngine::kSaveData);
  ASSERT_GT(data.size(), 2u);
  ExpectResponse(data, 0, 2, "token-1");
  ExpectResponse(data, 2, data.size() - 2, "token-2");

  plugin_->OnMessage(
      CreateRequest(pdf::mojom::SaveRequestType::kEdited, 0, 0, "token-1"));
  EXPECT_FALSE(plugin_->IsSaveDataBufferEmptyForTesting());
  plugin_->OnMessage(CreateRequest(pdf::mojom::SaveRequestType::kEdited, 2,
                                   data.size() - 2, "token-2"));
  EXPECT_TRUE(plugin_->IsSaveDataBufferEmptyForTesting());
  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginSaveInBlocksTest, ReleaseSaveBuffer) {
  plugin_->EnteredEditMode();
  plugin_->SetMaxSaveBufferSizeForTesting(2);

  base::span data(TestPDFiumEngine::kSaveData);
  ASSERT_GT(data.size(), 2u);
  ExpectResponse(data, 0, 2, "token-1");

  plugin_->OnMessage(
      CreateRequest(pdf::mojom::SaveRequestType::kEdited, 0, 0, "token-1"));
  EXPECT_FALSE(plugin_->IsSaveDataBufferEmptyForTesting());

  plugin_->OnMessage(
      base::Value::Dict().Set("type", "releaseSaveInBlockBuffers"));
  EXPECT_TRUE(plugin_->IsSaveDataBufferEmptyForTesting());

  pdf_receiver_.FlushForTesting();
}

#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
class PdfViewWebPluginSaveInBlocksToGoogleDriveTest
    : public PdfViewWebPluginSaveInBlocksTest {
 protected:
  void FreeHandler(mojo::Remote<pdf::mojom::SaveDataBufferHandler>& handler) {
    handler.reset();
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  std::pair<mojo::Remote<pdf::mojom::SaveDataBufferHandler>, uint32_t>
  GetSaveDataBufferHandler(pdf::mojom::SaveRequestType request_type) {
    base::test::TestFuture<pdf::mojom::SaveDataBufferHandlerGetResultPtr>
        future;
    plugin_->GetSaveDataBufferHandlerForDrive(
        request_type,
        future.GetCallback<pdf::mojom::SaveDataBufferHandlerGetResultPtr>());
    pdf::mojom::SaveDataBufferHandlerGetResultPtr result = future.Take();
    mojo::Remote<pdf::mojom::SaveDataBufferHandler> handler(
        std::move(result->handler));
    return {std::move(handler), result->total_file_size};
  }

  void ReadSaveDataBufferAndExpectResult(
      mojo::Remote<pdf::mojom::SaveDataBufferHandler>& handler,
      base::span<const uint8_t> expected_file_data,
      uint32_t offset,
      uint32_t block_size) {
    base::test::TestFuture<mojo_base::BigBuffer> read_future;
    handler->Read(offset, block_size,
                  read_future.GetCallback<mojo_base::BigBuffer>());
    mojo_base::BigBuffer block = read_future.Take();
    auto expected_block_data = expected_file_data.subspan(offset, block_size);
    EXPECT_THAT(block, ElementsAreArray(expected_block_data));
  }
};

TEST_F(PdfViewWebPluginSaveInBlocksToGoogleDriveTest, OriginalInOneBlock) {
  base::span<const uint8_t> data(TestPDFiumEngine::kLoadedData);
  auto [handler, total_file_size] =
      GetSaveDataBufferHandler(pdf::mojom::SaveRequestType::kOriginal);
  EXPECT_EQ(total_file_size, static_cast<uint32_t>(data.size()));
  ReadSaveDataBufferAndExpectResult(handler, data, 0, data.size());
  FreeHandler(handler);
  EXPECT_EQ(plugin_->GetSaveToDriveBufferHandlerReceiverSizeForTesting(), 0u);
}

TEST_F(PdfViewWebPluginSaveInBlocksToGoogleDriveTest, OriginalInMulipleBlocks) {
  plugin_->SetMaxSaveBufferSizeForTesting(3);

  base::span<const uint8_t> data(TestPDFiumEngine::kLoadedData);
  ASSERT_GT(data.size(), 3u);
  auto [handler, total_file_size] =
      GetSaveDataBufferHandler(pdf::mojom::SaveRequestType::kOriginal);
  EXPECT_EQ(total_file_size, static_cast<uint32_t>(data.size()));
  ReadSaveDataBufferAndExpectResult(handler, data, 0, 3);
  EXPECT_EQ(plugin_->GetSaveToDriveBufferHandlerReceiverSizeForTesting(), 1u);
  ReadSaveDataBufferAndExpectResult(handler, data, 3, data.size() - 3);
  FreeHandler(handler);
  EXPECT_EQ(plugin_->GetSaveToDriveBufferHandlerReceiverSizeForTesting(), 0u);
}

TEST_F(PdfViewWebPluginSaveInBlocksToGoogleDriveTest,
       GetNullptrForDataSizeGreaterThanIntMax) {
  EXPECT_CALL(*engine_ptr_, GetLoadedByteSize)
      .WillRepeatedly(Return(static_cast<uint32_t>(INT_MAX) + 1));
  base::test::TestFuture<pdf::mojom::SaveDataBufferHandlerGetResultPtr> future;
  plugin_->GetSaveDataBufferHandlerForDrive(
      pdf::mojom::SaveRequestType::kOriginal,
      future.GetCallback<pdf::mojom::SaveDataBufferHandlerGetResultPtr>());
  pdf::mojom::SaveDataBufferHandlerGetResultPtr result = future.Take();
  EXPECT_FALSE(result);
  EXPECT_EQ(plugin_->GetSaveToDriveBufferHandlerReceiverSizeForTesting(), 0u);
}

TEST_F(PdfViewWebPluginSaveInBlocksToGoogleDriveTest, EditedInOneBlock) {
  plugin_->EnteredEditMode();

  base::span<const uint8_t> data(TestPDFiumEngine::kSaveData);
  auto [handler, total_file_size] =
      GetSaveDataBufferHandler(pdf::mojom::SaveRequestType::kEdited);
  EXPECT_EQ(total_file_size, static_cast<uint32_t>(data.size()));
  ReadSaveDataBufferAndExpectResult(handler, data, 0, data.size());
  FreeHandler(handler);
  EXPECT_EQ(plugin_->GetSaveToDriveBufferHandlerReceiverSizeForTesting(), 0u);
}

TEST_F(PdfViewWebPluginSaveInBlocksToGoogleDriveTest, EditedInMultipleBlocks) {
  plugin_->EnteredEditMode();
  plugin_->SetMaxSaveBufferSizeForTesting(2);

  base::span<const uint8_t> data(TestPDFiumEngine::kSaveData);
  ASSERT_GT(data.size(), 2u);

  auto [handler, total_file_size] =
      GetSaveDataBufferHandler(pdf::mojom::SaveRequestType::kEdited);
  EXPECT_EQ(total_file_size, static_cast<uint32_t>(data.size()));
  ReadSaveDataBufferAndExpectResult(handler, data, 0, 2);
  EXPECT_EQ(plugin_->GetSaveToDriveBufferHandlerReceiverSizeForTesting(), 1u);

  ReadSaveDataBufferAndExpectResult(handler, data, 2, data.size() - 2);
  FreeHandler(handler);
  EXPECT_EQ(plugin_->GetSaveToDriveBufferHandlerReceiverSizeForTesting(), 0u);
}

TEST_F(PdfViewWebPluginSaveInBlocksToGoogleDriveTest,
       GetEditedInMultipleBlockForDriveAndSaveSimulatenously) {
  plugin_->EnteredEditMode();
  plugin_->SetMaxSaveBufferSizeForTesting(2);

  base::span<const uint8_t> data(TestPDFiumEngine::kSaveData);
  ASSERT_GT(data.size(), 2u);
  ExpectResponse(data, 0, 2, "token-1");
  ExpectResponse(data, 2, data.size() - 2, "token-2");

  auto [handler, total_file_size] =
      GetSaveDataBufferHandler(pdf::mojom::SaveRequestType::kEdited);
  EXPECT_EQ(total_file_size, static_cast<uint32_t>(data.size()));
  ReadSaveDataBufferAndExpectResult(handler, data, 0, 2);
  plugin_->OnMessage(
      CreateRequest(pdf::mojom::SaveRequestType::kEdited, 0, 0, "token-1"));
  EXPECT_EQ(plugin_->GetSaveToDriveBufferHandlerReceiverSizeForTesting(), 1u);
  EXPECT_FALSE(plugin_->IsSaveDataBufferEmptyForTesting());

  ReadSaveDataBufferAndExpectResult(handler, data, 2, data.size() - 2);
  FreeHandler(handler);
  EXPECT_EQ(plugin_->GetSaveToDriveBufferHandlerReceiverSizeForTesting(), 0u);
  EXPECT_FALSE(plugin_->IsSaveDataBufferEmptyForTesting());

  plugin_->OnMessage(CreateRequest(pdf::mojom::SaveRequestType::kEdited, 2,
                                   data.size() - 2, "token-2"));
  EXPECT_EQ(plugin_->GetSaveToDriveBufferHandlerReceiverSizeForTesting(), 0u);
  EXPECT_TRUE(plugin_->IsSaveDataBufferEmptyForTesting());
  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginSaveInBlocksToGoogleDriveTest,
       MultipleHandlersEditedInMultipleBlocks) {
  plugin_->EnteredEditMode();
  plugin_->SetMaxSaveBufferSizeForTesting(3);

  base::span<const uint8_t> data(TestPDFiumEngine::kSaveData);
  ASSERT_GT(data.size(), 3u);
  base::span<const uint8_t> data2(kSaveDataBuffer);
  ASSERT_GT(data2.size(), 3u);

  EXPECT_CALL(*engine_ptr_, GetSaveData)
      .WillOnce(Return(std::vector<uint8_t>(data.begin(), data.end())))
      .WillOnce(Return(std::vector<uint8_t>(data2.begin(), data2.end())));

  auto [handler, total_file_size] =
      GetSaveDataBufferHandler(pdf::mojom::SaveRequestType::kEdited);
  EXPECT_EQ(total_file_size, data.size());
  ReadSaveDataBufferAndExpectResult(handler, data, 0, 2);
  EXPECT_EQ(plugin_->GetSaveToDriveBufferHandlerReceiverSizeForTesting(), 1u);

  auto [handler2, total_file_size2] =
      GetSaveDataBufferHandler(pdf::mojom::SaveRequestType::kEdited);
  EXPECT_EQ(total_file_size2, data2.size());
  ReadSaveDataBufferAndExpectResult(handler2, data2, 0, 2);
  EXPECT_EQ(plugin_->GetSaveToDriveBufferHandlerReceiverSizeForTesting(), 2u);

  ReadSaveDataBufferAndExpectResult(handler, data, 2, data.size() - 2);
  FreeHandler(handler);
  EXPECT_EQ(plugin_->GetSaveToDriveBufferHandlerReceiverSizeForTesting(), 1u);

  ReadSaveDataBufferAndExpectResult(handler2, data2, 2, data2.size() - 2);
  FreeHandler(handler2);
  EXPECT_EQ(plugin_->GetSaveToDriveBufferHandlerReceiverSizeForTesting(), 0u);
}
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

class PdfViewWebPluginSubmitFormTest
    : public PdfViewWebPluginWithoutInitializeTest {
 protected:
  void SubmitForm(const std::string& url, std::string_view form_data = "data") {
    EXPECT_TRUE(plugin_->InitializeForTesting());

    EXPECT_CALL(*client_ptr_, CreateAssociatedURLLoader).WillOnce([this]() {
      auto associated_loader =
          std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
      EXPECT_CALL(*associated_loader, LoadAsynchronously)
          .WillOnce([this](const blink::WebURLRequest& request,
                           blink::WebAssociatedURLLoaderClient* /*client*/) {
            // TODO(crbug.com/40224475): The `UrlLoader` created by `LoadUrl()`
            // and `SubmitForm()` shouldn't use different ownership semantics.
            // The loader created by `SubmitForm()` is owned by the plugin, and
            // cannot leak past the destruction of the plugin.
            request_.CopyFrom(request);
          });
      return associated_loader;
    });

    plugin_->SubmitForm(url, form_data.data(), form_data.size());
  }

  void SubmitFailingForm(const std::string& url) {
    EXPECT_TRUE(plugin_->InitializeForTesting());

    EXPECT_CALL(*client_ptr_, CreateAssociatedURLLoader).Times(0);

    constexpr std::string_view kFormData = "form data";
    plugin_->SubmitForm(url, kFormData.data(), kFormData.size());
  }

  blink::WebURLRequest request_;
};

TEST_F(PdfViewWebPluginSubmitFormTest, RequestMethod) {
  SetUpPluginWithUrl("https://www.example.com/path/to/the.pdf");

  SubmitForm(/*url=*/"");

  EXPECT_EQ(request_.HttpMethod().Utf8(), "POST");
}

TEST_F(PdfViewWebPluginSubmitFormTest, RequestBody) {
  SetUpPluginWithUrl("https://www.example.com/path/to/the.pdf");

  constexpr std::string_view kFormData = "form data";
  SubmitForm(/*url=*/"", kFormData);

  blink::WebHTTPBody::Element element;
  EXPECT_EQ(request_.HttpBody().ElementCount(), 1u);
  ASSERT_TRUE(request_.HttpBody().ElementAt(0, element));
  ASSERT_EQ(element.type, blink::HTTPBodyElementType::kTypeData);
  EXPECT_THAT(element.data.Copy(), testing::ElementsAreArray(kFormData));
}

TEST_F(PdfViewWebPluginSubmitFormTest, RelativeUrl) {
  SetUpPluginWithUrl("https://www.example.com/path/to/the.pdf");

  SubmitForm("relative_endpoint");

  EXPECT_EQ(request_.Url().GetString().Utf8(),
            "https://www.example.com/path/to/relative_endpoint");
}

TEST_F(PdfViewWebPluginSubmitFormTest, NoRelativeUrl) {
  SetUpPluginWithUrl("https://www.example.com/path/to/the.pdf");

  SubmitForm("");

  EXPECT_EQ(request_.Url().GetString().Utf8(),
            "https://www.example.com/path/to/the.pdf");
}

TEST_F(PdfViewWebPluginSubmitFormTest, AbsoluteUrl) {
  SetUpPluginWithUrl("https://a.example.com/path/to/the.pdf");

  SubmitForm("https://b.example.com/relative_endpoint");

  EXPECT_EQ(request_.Url().GetString().Utf8(),
            "https://b.example.com/relative_endpoint");
}

TEST_F(PdfViewWebPluginSubmitFormTest, RelativeUrlInvalidDocumentUrl) {
  SetUpPluginWithUrl("https://www.%B%Ad.com/path/to/the.pdf");

  SubmitFailingForm("relative_endpoint");
}

TEST_F(PdfViewWebPluginSubmitFormTest, AbsoluteUrlInvalidDocumentUrl) {
  SetUpPluginWithUrl("https://www.%B%Ad.com/path/to/the.pdf");

  SubmitFailingForm("https://wwww.example.com");
}

class PdfViewWebPluginPrintTest : public PdfViewWebPluginTest {
 protected:
  void SetUp() override {
    PdfViewWebPluginTest::SetUp();

    // Size must be at least 1 for conversion to `SkMemoryStream`.
    ON_CALL(*engine_ptr_, PrintPages)
        .WillByDefault(Return(std::vector<uint8_t>(1)));

    canvas_.sk_canvas()->SetPrintingMetafile(&metafile_);
  }

  printing::MetafileSkia metafile_;
};

TEST_F(PdfViewWebPluginPrintTest, HighQuality) {
  EXPECT_CALL(*engine_ptr_,
              HasPermission(DocumentPermission::kPrintHighQuality))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kPrintLowQuality))
      .WillRepeatedly(Return(true));
  ASSERT_EQ(static_cast<int>(TestPDFiumEngine::kPageNumber),
            plugin_->PrintBegin(blink::WebPrintParams()));

  EXPECT_CALL(
      *engine_ptr_,
      PrintPages(ElementsAre(0),
                 Field(&blink::WebPrintParams::rasterize_pdf, IsFalse())));
  plugin_->PrintPage(0, canvas_.sk_canvas());
  plugin_->PrintEnd();
}

TEST_F(PdfViewWebPluginPrintTest, HighQualityRasterized) {
  EXPECT_CALL(*engine_ptr_,
              HasPermission(DocumentPermission::kPrintHighQuality))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kPrintLowQuality))
      .WillRepeatedly(Return(true));

  blink::WebPrintParams params;
  params.rasterize_pdf = true;
  ASSERT_EQ(static_cast<int>(TestPDFiumEngine::kPageNumber),
            plugin_->PrintBegin(params));

  EXPECT_CALL(
      *engine_ptr_,
      PrintPages(ElementsAre(0),
                 Field(&blink::WebPrintParams::rasterize_pdf, IsTrue())));
  plugin_->PrintPage(0, canvas_.sk_canvas());
  plugin_->PrintEnd();
}

// Regression test for crbug.com/1307219.
TEST_F(PdfViewWebPluginPrintTest, LowQuality) {
  EXPECT_CALL(*engine_ptr_,
              HasPermission(DocumentPermission::kPrintHighQuality))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kPrintLowQuality))
      .WillRepeatedly(Return(true));
  ASSERT_EQ(static_cast<int>(TestPDFiumEngine::kPageNumber),
            plugin_->PrintBegin(blink::WebPrintParams()));

  EXPECT_CALL(
      *engine_ptr_,
      PrintPages(ElementsAre(0),
                 Field(&blink::WebPrintParams::rasterize_pdf, IsTrue())));
  plugin_->PrintPage(0, canvas_.sk_canvas());
  plugin_->PrintEnd();
}

// Regression test for crbug.com/1307219.
TEST_F(PdfViewWebPluginPrintTest, LowQualityRasterized) {
  EXPECT_CALL(*engine_ptr_,
              HasPermission(DocumentPermission::kPrintHighQuality))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*engine_ptr_, HasPermission(DocumentPermission::kPrintLowQuality))
      .WillRepeatedly(Return(true));

  blink::WebPrintParams params;
  params.rasterize_pdf = true;
  ASSERT_EQ(static_cast<int>(TestPDFiumEngine::kPageNumber),
            plugin_->PrintBegin(params));

  EXPECT_CALL(
      *engine_ptr_,
      PrintPages(ElementsAre(0),
                 Field(&blink::WebPrintParams::rasterize_pdf, IsTrue())));
  plugin_->PrintPage(0, canvas_.sk_canvas());
  plugin_->PrintEnd();
}

TEST_F(PdfViewWebPluginPrintTest, Disabled) {
  EXPECT_EQ(0, plugin_->PrintBegin(blink::WebPrintParams()));
}

TEST_F(PdfViewWebPluginPrintTest, DisabledRasterized) {
  blink::WebPrintParams params;
  params.rasterize_pdf = true;
  EXPECT_EQ(0, plugin_->PrintBegin(params));
}

class PdfViewWebPluginPrintPreviewTest : public PdfViewWebPluginTest {
 protected:
  void SetUpClient() override {
    EXPECT_CALL(*client_ptr_, GetEmbedderOriginString)
        .WillRepeatedly(Return("chrome://print/"));
  }
};

TEST_F(PdfViewWebPluginPrintPreviewTest, HandleResetPrintPreviewModeMessage) {
  EXPECT_CALL(*client_ptr_, CreateEngine)
      .WillOnce([](PDFiumEngineClient* client,
                   PDFiumFormFiller::ScriptOption script_option) {
        EXPECT_EQ(PDFiumFormFiller::ScriptOption::kNoJavaScript, script_option);

        auto engine = std::make_unique<NiceMock<TestPDFiumEngine>>(client);
        EXPECT_CALL(*engine, ZoomUpdated);
        EXPECT_CALL(*engine, PageOffsetUpdated);
        EXPECT_CALL(*engine, PluginSizeUpdated);
        EXPECT_CALL(*engine, SetGrayscale(false));
        return engine;
      });

  OnMessageWithEngineUpdate(ParseMessage(R"({
    "type": "resetPrintPreviewMode",
    "url": "chrome-untrusted://print/0/0/print.pdf",
    "grayscale": false,
    "pageCount": 1,
  })"));
}

TEST_F(PdfViewWebPluginPrintPreviewTest,
       HandleResetPrintPreviewModeMessageForPdf) {
  EXPECT_CALL(*client_ptr_, CreateEngine)
      .WillOnce([](PDFiumEngineClient* client,
                   PDFiumFormFiller::ScriptOption script_option) {
        EXPECT_EQ(PDFiumFormFiller::ScriptOption::kNoJavaScript, script_option);

        return std::make_unique<NiceMock<TestPDFiumEngine>>(client);
      });

  // The UI ID of 1 in the URL is arbitrary.
  // The page index value of -1, AKA `kCompletePDFIndex`, is required for PDFs.
  OnMessageWithEngineUpdate(ParseMessage(R"({
    "type": "resetPrintPreviewMode",
    "url": "chrome-untrusted://print/1/-1/print.pdf",
    "grayscale": false,
    "pageCount": 0,
  })"));

  EXPECT_CALL(*client_ptr_, PostMessage).Times(AnyNumber());
  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(R"({
    "type": "printPreviewLoaded",
  })")));
  plugin_->DocumentLoadComplete();
  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginPrintPreviewTest,
       HandleResetPrintPreviewModeMessageSetGrayscale) {
  EXPECT_CALL(*client_ptr_, CreateEngine)
      .WillOnce([](PDFiumEngineClient* client,
                   PDFiumFormFiller::ScriptOption /*script_option*/) {
        auto engine = std::make_unique<NiceMock<TestPDFiumEngine>>(client);
        EXPECT_CALL(*engine, SetGrayscale(true));
        return engine;
      });

  OnMessageWithEngineUpdate(ParseMessage(R"({
    "type": "resetPrintPreviewMode",
    "url": "chrome-untrusted://print/0/0/print.pdf",
    "grayscale": true,
    "pageCount": 1,
  })"));
}

TEST_F(PdfViewWebPluginPrintPreviewTest, DocumentLoadComplete) {
  OnMessageWithEngineUpdate(ParseMessage(R"({
    "type": "resetPrintPreviewMode",
    "url": "chrome-untrusted://print/0/0/print.pdf",
    "grayscale": false,
    "pageCount": 1,
  })"));

  EXPECT_CALL(*client_ptr_, RecordComputedAction("PDF.LoadSuccess"));
  EXPECT_CALL(*client_ptr_, PostMessage);
  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(R"({
    "type": "formFocusChange",
    "focused": "none",
  })")));
  ExpectUpdateTextInputState(blink::WebTextInputType::kWebTextInputTypeNone);
  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(R"({
    "type": "printPreviewLoaded",
  })")));
  EXPECT_CALL(*accessibility_data_handler_ptr_, SetAccessibilityDocInfo)
      .Times(0);
  EXPECT_CALL(*client_ptr_, DidStopLoading).Times(0);
  EXPECT_CALL(pdf_host_, UpdateContentRestrictions).Times(0);
  plugin_->DocumentLoadComplete();

  EXPECT_EQ(PdfViewWebPlugin::DocumentLoadState::kComplete,
            plugin_->document_load_state_for_testing());
  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginPrintPreviewTest,
       DocumentLoadProgressResetByResetPrintPreviewModeMessage) {
  plugin_->DocumentLoadProgress(2, 100);

  OnMessageWithEngineUpdate(ParseMessage(R"({
    "type": "resetPrintPreviewMode",
    "url": "chrome-untrusted://print/123/0/print.pdf",
    "grayscale": false,
    "pageCount": 2,
  })"));

  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(R"({
    "type": "loadProgress",
    "progress": 3.0,
  })")));
  plugin_->DocumentLoadProgress(3, 100);
}

TEST_F(PdfViewWebPluginPrintPreviewTest,
       DocumentLoadProgressNotResetByLoadPreviewPageMessage) {
  OnMessageWithEngineUpdate(ParseMessage(R"({
    "type": "resetPrintPreviewMode",
    "url": "chrome-untrusted://print/123/0/print.pdf",
    "grayscale": false,
    "pageCount": 2,
  })"));

  plugin_->DocumentLoadProgress(2, 100);

  plugin_->OnMessage(ParseMessage(R"({
    "type": "loadPreviewPage",
    "url": "chrome-untrusted://print/123/1/print.pdf",
    "index": 1,
  })"));

  EXPECT_CALL(*client_ptr_, PostMessage).Times(0);
  plugin_->DocumentLoadProgress(3, 100);
}

TEST_F(PdfViewWebPluginPrintPreviewTest,
       HandleViewportMessageScrollRightToLeft) {
  EXPECT_CALL(*engine_ptr_, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  EXPECT_CALL(*engine_ptr_, ScrolledToXPosition(14));
  EXPECT_CALL(*engine_ptr_, ScrolledToYPosition(3));

  plugin_->OnMessage(ParseMessage(R"({
    "type": "viewport",
    "userInitiated": false,
    "zoom": 1,
    "layoutOptions": {
      "direction": 1,
      "defaultPageOrientation": 0,
      "twoUpViewEnabled": false,
    },
    "xOffset": -2,
    "yOffset": 3,
    "pinchPhase": 0,
  })"));
}

#if BUILDFLAG(ENABLE_PDF_INK2)
class PdfViewWebPluginInkTest
    : public PdfViewWebPluginTest,
      public testing::WithParamInterface<InkTestVariation> {
 public:
  void SetUp() override {
    // Feature and parameters need to be initialized first as it impacts plugin
    // setup in PdfViewWebPluginTest::SetUp().
    feature_list_.InitAndEnableFeatureWithParameters(
        chrome_pdf::features::kPdfInk2,
        {{features::kPdfInk2TextAnnotations.name,
          base::ToString(UseTextAnnotations())},
         {features::kPdfInk2TextHighlighting.name,
          base::ToString(UseTextHighlighting())}});

    PdfViewWebPluginTest::SetUp();
  }

 protected:
  static constexpr gfx::PointF kDiagonalStrokeStartPosition{95.0f, 85.0f};
  static constexpr gfx::PointF kDiagonalStrokeMiddlePosition{72.5f, 65.0f};
  static constexpr gfx::PointF kDiagonalStrokeEndPosition{50.0f, 45.0f};

  struct DiagonalStrokeEvents {
    blink::WebTouchEvent start_event;
    blink::WebTouchEvent move_event1;
    blink::WebTouchEvent move_event2;
    blink::WebTouchEvent end_event;
  };

  bool UseTextAnnotations() const { return GetParam().use_text_annotations; }
  bool UseTextHighlighting() const { return GetParam().use_text_highlighting; }

  void SetUpWithTrivialInkStrokes() {
    // Set up the engine so the plugin can draw strokes. The exact strokes do
    // not matter.
    EXPECT_CALL(*engine_ptr_, HandleInputEvent).Times(0);
    ON_CALL(*engine_ptr_, GetPageContentsRect)
        .WillByDefault(
            Return(gfx::Rect(/*x=*/0, /*y=*/0, /*width=*/100, /*height=*/50)));
    ON_CALL(*engine_ptr_, GetPageSizeInPoints)
        .WillByDefault(Return(gfx::SizeF(75.0f, 37.5f)));
    ON_CALL(*engine_ptr_, GetThumbnailSize)
        .WillByDefault(Return(gfx::Size(50, 25)));
    ON_CALL(*engine_ptr_, IsPageVisible).WillByDefault(Return(true));

    // Draw some trivial strokes.
    plugin_->OnMessage(
        CreateSetAnnotationModeMessageForTesting(InkAnnotationMode::kDraw));
    TestSendInputEvent(CreateLeftClickWebMouseEventAtPosition({10, 10}),
                       blink::WebInputEventResult::kHandledApplication);
    TestSendInputEvent(CreateLeftClickWebMouseUpEventAtPosition({20, 20}),
                       blink::WebInputEventResult::kHandledApplication);
  }

  void SendThumbnail(std::string_view message_id, const gfx::SizeF& page_size) {
    auto reply = base::Value::Dict()
                     .Set("type", "getThumbnailReply")
                     .Set("messageId", message_id);
    plugin_->SendThumbnailForTesting(
        std::move(reply), /*page_index=*/0,
        Thumbnail(page_size, /*device_pixel_ratio=*/1));
  }

  // Helper method to test PageIndexFromPoint() and VisiblePageIndexFromPoint().
  // If `expected_visible`, then VisiblePageIndexFromPoint() should have a page
  // index of `expected_page_index`, otherwise -1.
  void TestPageIndexFromPoint(const gfx::PointF& point,
                              int expected_page_index,
                              bool expected_visible) {
    PdfInkModuleClient* ink_module_client =
        plugin_->ink_module_client_for_testing();
    EXPECT_EQ(expected_page_index,
              ink_module_client->PageIndexFromPoint(point));
    EXPECT_EQ(expected_visible ? expected_page_index : -1,
              ink_module_client->VisiblePageIndexFromPoint(point));
  }

  void TestInProgressDraw(
      base::FilePath::StringViewType expected_filename,
      const blink::WebInputEvent& down_event,
      base::span<const blink::WebInputEvent* const> move_events,
      const blink::WebInputEvent& up_event) {
    plugin_->set_in_paint_for_testing(true);
    constexpr gfx::Rect kScreenRect(kCanvasSize);
    constexpr gfx::SizeF kPageSizeInPoints(
        kCanvasSize.width() * kUnitConversionFactorPixelsToPoints,
        kCanvasSize.height() * kUnitConversionFactorPixelsToPoints);
    ON_CALL(*engine_ptr_, GetPageContentsRect)
        .WillByDefault(Return(kScreenRect));
    ON_CALL(*engine_ptr_, GetPageSizeInPoints)
        .WillByDefault(Return(kPageSizeInPoints));
    ON_CALL(*engine_ptr_, GetThumbnailSize)
        .WillByDefault(Return(gfx::Size(50, 50)));
    ON_CALL(*engine_ptr_, IsPageVisible).WillByDefault(Return(true));

    UpdatePluginGeometry(/*device_scale=*/1.0f, kScreenRect);

    // The canvas starts blank.
    canvas_.DrawColor(SK_ColorWHITE);
    plugin_->Paint(canvas_.sk_canvas(), kScreenRect);
    SkBitmap blank_bitmap =
        GenerateExpectedBitmapForPaint(kScreenRect, SK_ColorWHITE);
    EXPECT_TRUE(cc::MatchesBitmap(canvas_.GetBitmap(), blank_bitmap,
                                  cc::ExactPixelComparator()));

    // Start to draw a stroke.  There should not be a call to apply the stroke
    // until drawing is finished.
    EXPECT_CALL(*engine_ptr_, ApplyStroke(_, _, _)).Times(0);
    // The final imaging for a stroke saved to a PDF should match what was final
    // drawn result when it was in-progress.
    TestSendInputEvent(down_event,
                       blink::WebInputEventResult::kHandledApplication);
    for (const auto* move_event : move_events) {
      TestSendInputEvent(*move_event,
                         blink::WebInputEventResult::kHandledApplication);
    }

    // Draw the canvas for the in-progress stroke.
    plugin_->Paint(canvas_.sk_canvas(), kScreenRect);
    const base::FilePath stroked_image_png_file =
        GetInkTestDataFilePath(expected_filename);
    EXPECT_TRUE(
        MatchesPngFile(*canvas_.GetBitmap().asImage(), stroked_image_png_file));

    // Finish the stroke.  After a stroke is finished there is nothing more to
    // be drawn by PdfInkModule, as the completed stroke is provided by a
    // callback to be applied to a PDF page.
    testing::Mock::VerifyAndClearExpectations(engine_ptr_);
    EXPECT_CALL(*engine_ptr_, ApplyStroke(/*page_index=*/0, InkStrokeId(0), _));
    TestSendInputEvent(up_event,
                       blink::WebInputEventResult::kHandledApplication);

    // Updating of `PdfViewWebPlugin::snapshot_` does not happen automatically
    // on the invalidate call, but later after the tasks PaintManager posted
    // have a chance to run.  This means painting uses the last snapshot, which
    // does not include the last Ink stroke.  This results in the most recent
    // stroke disappearing, causing a flash for the user unless the snapshot
    // from the most recent stroke is reused.
    plugin_->Paint(canvas_.sk_canvas(), kScreenRect);
    EXPECT_TRUE(
        MatchesPngFile(*canvas_.GetBitmap().asImage(), stroked_image_png_file));
    EXPECT_TRUE(plugin_->HasInkInputsSnapshotForTesting());

    // Simulate how the snapshot eventually gets updated, after all necessary
    // tasks that normally happen from the PaintManager finally complete.  That
    // results in a blank canvas here for this test, as PdfViewWebPlugin no
    // longer uses the last Ink rendering snapshot for painting, and
    // ApplyStroke() was mocked out so there is nothing to draw from the PDF
    // engine.
    plugin_->UpdateSnapshot(CreateSkiaImageForTesting(
        plugin_->GetPluginRectForTesting().size(), SK_ColorWHITE));
    plugin_->Paint(canvas_.sk_canvas(), kScreenRect);
    EXPECT_TRUE(cc::MatchesBitmap(canvas_.GetBitmap(), blank_bitmap,
                                  cc::ExactPixelComparator()));
    EXPECT_FALSE(plugin_->HasInkInputsSnapshotForTesting());
  }

  // TODO(thestig): Deduplicate with CreateTouchEvent() code in other tests.
  static blink::WebTouchEvent CreateTouchEvent(blink::WebInputEvent::Type type,
                                               const gfx::PointF& point) {
    constexpr int kNoModifiers = 0;
    blink::WebTouchEvent touch_event(
        type, kNoModifiers, blink::WebInputEvent::GetStaticTimeStampForTests());
    touch_event.touches[0].SetPositionInWidget(point);
    touch_event.touches_length = 1;
    return touch_event;
  }

  static blink::WebTouchEvent CreatePenEvent(blink::WebInputEvent::Type type,
                                             const gfx::PointF& point) {
    blink::WebTouchEvent pen_event = CreateTouchEvent(type, point);
    pen_event.touches[0].pointer_type =
        blink::WebPointerProperties::PointerType::kPen;
    return pen_event;
  }

  DiagonalStrokeEvents CreateInputsForDiagonalInProgressStrokeTest(
      bool is_touch) {
    auto create_func = is_touch ? &PdfViewWebPluginInkTest::CreateTouchEvent
                                : &PdfViewWebPluginInkTest::CreatePenEvent;
    DiagonalStrokeEvents events{
        .start_event = create_func(blink::WebInputEvent::Type::kTouchStart,
                                   kDiagonalStrokeStartPosition),
        .move_event1 = create_func(blink::WebInputEvent::Type::kTouchMove,
                                   kDiagonalStrokeMiddlePosition),
        .move_event2 = create_func(blink::WebInputEvent::Type::kTouchMove,
                                   kDiagonalStrokeEndPosition),
        .end_event = create_func(blink::WebInputEvent::Type::kTouchEnd,
                                 kDiagonalStrokeEndPosition)};
    events.move_event1.SetTimeStamp(events.start_event.TimeStamp() +
                                    base::Seconds(0.1f));
    events.move_event2.SetTimeStamp(events.move_event1.TimeStamp() +
                                    base::Seconds(0.5f));
    events.end_event.SetTimeStamp(events.move_event2.TimeStamp() +
                                  base::Seconds(0.1f));
    return events;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(PdfViewWebPluginInkTest, Invalidate) {
  plugin_->set_in_paint_for_testing(true);
  EXPECT_EQ(0u, plugin_->deferred_invalidates_for_testing().size());
  SetUpWithTrivialInkStrokes();
  EXPECT_EQ(3u, plugin_->deferred_invalidates_for_testing().size());
}

TEST_P(PdfViewWebPluginInkTest, LoadV2InkPathsForPageAndUpdateShapeActive) {
  const std::map<InkModeledShapeId, ink::PartitionedMesh> kEmptyMap;
  const std::map<InkModeledShapeId, ink::PartitionedMesh> kMap0{
      {InkModeledShapeId(0), ink::PartitionedMesh()},
  };
  const std::map<InkModeledShapeId, ink::PartitionedMesh> kMap1{
      {InkModeledShapeId(1), ink::PartitionedMesh()},
      {InkModeledShapeId(2), ink::PartitionedMesh()},
  };
  const std::map<InkModeledShapeId, ink::PartitionedMesh> kMap2{
      {InkModeledShapeId(3), ink::PartitionedMesh()},
      {InkModeledShapeId(4), ink::PartitionedMesh()},
      {InkModeledShapeId(5), ink::PartitionedMesh()},
  };

  EXPECT_CALL(*engine_ptr_, LoadV2InkPathsForPage(testing::Lt(12)))
      .Times(12)
      .WillOnce(Return(kMap0))
      .WillOnce(Return(kMap1))
      .WillRepeatedly(Return(kEmptyMap));
  EXPECT_CALL(*engine_ptr_, LoadV2InkPathsForPage(12)).WillOnce(Return(kMap2));

  const PdfInkModuleClient::DocumentV2InkPathShapesMap result =
      plugin_->ink_module_client_for_testing()->LoadV2InkPathsFromPdf();
  EXPECT_THAT(
      result,
      ElementsAre(Pair(0, ElementsAre(Pair(InkModeledShapeId(0), _))),
                  Pair(1, ElementsAre(Pair(InkModeledShapeId(1), _),
                                      Pair(InkModeledShapeId(2), _))),
                  Pair(12, ElementsAre(Pair(InkModeledShapeId(3), _),
                                       Pair(InkModeledShapeId(4), _),
                                       Pair(InkModeledShapeId(5), _)))));

  EXPECT_CALL(*engine_ptr_, UpdateShapeActive(0, InkModeledShapeId(0), false));
  plugin_->ink_module_client_for_testing()->UpdateShapeActive(
      /*page_index=*/0, InkModeledShapeId(0),
      /*active=*/false);
}

TEST_P(PdfViewWebPluginInkTest, SendThumbnailUpdatesInkThumbnail) {
  SetUpWithTrivialInkStrokes();

  EXPECT_CALL(*client_ptr_, PostMessage)
      .WillOnce([](const base::Value::Dict& dict) {
        auto expected = base::test::ParseJsonDict(R"({
            "type": "getThumbnailReply",
            "messageId": "foo",
            "width": 216,
            "height": 108,
        })");
        EXPECT_THAT(dict, base::test::DictionaryHasValues(expected));

        // Test `dict` contains the image data, but not the exact value.
        const auto* blob = dict.FindBlob("imageData");
        ASSERT_TRUE(blob);
        EXPECT_FALSE(blob->empty());
      })
      .WillOnce([](const base::Value::Dict& dict) {
        auto expected = base::test::ParseJsonDict(R"({
            "type": "updateInk2Thumbnail",
            "pageNumber": 1,
            "width": 216,
            "height": 108,
        })");
        EXPECT_THAT(dict, base::test::DictionaryHasValues(expected));

        // Test `dict` contains the image data, but not the exact value.
        const auto* blob = dict.FindBlob("imageData");
        ASSERT_TRUE(blob);
        EXPECT_FALSE(blob->empty());
      });
  SendThumbnail(/*message_id=*/"foo", /*page_size=*/{50, 25});
}

TEST_P(PdfViewWebPluginInkTest, SendThumbnailWithNoStrokes) {
  EXPECT_CALL(*client_ptr_, PostMessage)
      .WillOnce([](const base::Value::Dict& dict) {
        auto expected = base::test::ParseJsonDict(R"({
            "type": "getThumbnailReply",
            "messageId": "foo",
            "width": 216,
            "height": 108,
        })");
        EXPECT_THAT(dict, base::test::DictionaryHasValues(expected));

        // Test `dict` contains the image data, but not the exact value.
        const auto* blob = dict.FindBlob("imageData");
        ASSERT_TRUE(blob);
        EXPECT_FALSE(blob->empty());
      });
  SendThumbnail(/*message_id=*/"foo", /*page_size=*/{50, 25});
}

TEST_P(PdfViewWebPluginInkTest, GetCursor) {
  plugin_->set_cursor_type_for_testing(ui::mojom::CursorType::kPointer);
  EXPECT_EQ(ui::mojom::CursorType::kPointer,
            plugin_->ink_module_client_for_testing()->GetCursor().type());

  plugin_->set_cursor_type_for_testing(ui::mojom::CursorType::kIBeam);
  EXPECT_EQ(ui::mojom::CursorType::kIBeam,
            plugin_->ink_module_client_for_testing()->GetCursor().type());
}

TEST_P(PdfViewWebPluginInkTest, UpdateCursor) {
  UpdatePluginGeometryWithoutWaiting(2.0f, {0, 0, 20, 20});

  ON_CALL(*engine_ptr_, HandleInputEvent)
      .WillByDefault([this](const blink::WebInputEvent& event) -> bool {
        plugin_->UpdateCursor(ui::mojom::CursorType::kPointer);
        return false;
      });

  blink::WebMouseEvent mouse_event;
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseMove);
  mouse_event.SetPositionInWidget(10.0f, 20.0f);

  ui::Cursor cursor;
  EXPECT_EQ(ui::mojom::CursorType::kNull, cursor.type());
  cursor =
      TestSendInputEvent(mouse_event, blink::WebInputEventResult::kNotHandled);
  EXPECT_EQ(ui::mojom::CursorType::kPointer, cursor.type());

  plugin_->OnMessage(
      CreateSetAnnotationModeMessageForTesting(InkAnnotationMode::kDraw));
  cursor =
      TestSendInputEvent(mouse_event, blink::WebInputEventResult::kNotHandled);
  EXPECT_EQ(ui::mojom::CursorType::kCustom, cursor.type());

  plugin_->OnMessage(
      CreateSetAnnotationModeMessageForTesting(InkAnnotationMode::kOff));
  cursor =
      TestSendInputEvent(mouse_event, blink::WebInputEventResult::kNotHandled);
  EXPECT_EQ(ui::mojom::CursorType::kPointer, cursor.type());
}

TEST_P(PdfViewWebPluginInkTest, ClearSelection) {
  EXPECT_CALL(*engine_ptr_, ClearTextSelection);
  plugin_->ink_module_client_for_testing()->ClearSelection();
}

TEST_P(PdfViewWebPluginInkTest, ExtendSelectionByPoint) {
  EXPECT_CALL(*engine_ptr_, ExtendSelectionByPoint(gfx::PointF(10.5f, 20.1f)));
  plugin_->ink_module_client_for_testing()->ExtendSelectionByPoint(
      gfx::PointF(10.5f, 20.1f));
}

TEST_P(PdfViewWebPluginInkTest, GetCanonicalToPdfTransform) {
  static constexpr auto kTransform = gfx::Transform::MakeScale(3.0f);
  ON_CALL(*engine_ptr_, GetCanonicalToPdfTransform)
      .WillByDefault(Return(kTransform));
  EXPECT_EQ(kTransform, plugin_->ink_module_client_for_testing()
                            ->GetCanonicalToPdfTransform(/*page_index=*/0));
}

TEST_P(PdfViewWebPluginInkTest, GetPageSizeInPoints) {
  SetUpWithTrivialInkStrokes();
  EXPECT_EQ(gfx::SizeF(75.0f, 37.5f),
            plugin_->ink_module_client_for_testing()->GetPageSizeInPoints(
                /*page_index=*/0));
}

TEST_P(PdfViewWebPluginInkTest, GetSelectionRectMap) {
  static constexpr PdfRect kRect(10, 20, 40, 60);
  PdfInkModuleClient::SelectionRectMap selection_map{{0, {kRect}}};
  ON_CALL(*engine_ptr_, GetSelectionRectMap)
      .WillByDefault(Return(selection_map));
  EXPECT_THAT(plugin_->ink_module_client_for_testing()->GetSelectionRectMap(),
              ElementsAre(Pair(0, ElementsAre(kRect))));
}

TEST_P(PdfViewWebPluginInkTest, GetThumbnailSize) {
  SetUpWithTrivialInkStrokes();
  EXPECT_EQ(gfx::Size(50, 25),
            plugin_->ink_module_client_for_testing()->GetThumbnailSize(
                /*page_index=*/0));
}

TEST_P(PdfViewWebPluginInkTest, GetZoom) {
  // Demonstrate that default zoom is identity.
  EXPECT_EQ(1.0f, plugin_->ink_module_client_for_testing()->GetZoom());

  // Verify that changing the plugin zoom shows effect.
  EXPECT_CALL(*engine_ptr_, ZoomUpdated(2.0f));
  plugin_->OnMessage(ParseMessage(R"({
    "type": "viewport",
    "userInitiated": false,
    "zoom": 2,
    "layoutOptions": {
      "direction": 0,
      "defaultPageOrientation": 0,
      "twoUpViewEnabled": false,
    },
    "xOffset": 0,
    "yOffset": 0,
    "pinchPhase": 0,
  })"));
  EXPECT_EQ(2.0f, plugin_->ink_module_client_for_testing()->GetZoom());

  // Verify that changing the platform device scale shows effect.
  ON_CALL(*client_ptr_, DeviceScaleFactor).WillByDefault(Return(1.25f));
  EXPECT_CALL(*engine_ptr_, ZoomUpdated(2.5f));
  constexpr gfx::Rect kWindowRect(12, 24, 36, 48);
  plugin_->UpdateGeometry(kWindowRect, kWindowRect, kWindowRect,
                          /*is_visible=*/true);
  EXPECT_EQ(2.5f, plugin_->ink_module_client_for_testing()->GetZoom());
}

TEST_P(PdfViewWebPluginInkTest, RequestThumbnail) {
  EXPECT_CALL(*engine_ptr_, RequestThumbnail)
      .WillOnce([](int page_index, float device_pixel_ratio,
                   SendThumbnailCallback send_callback) {
        EXPECT_EQ(0, page_index);
        EXPECT_EQ(1.0f, device_pixel_ratio);
        std::move(send_callback)
            .Run(Thumbnail(gfx::SizeF(612, 792), device_pixel_ratio));
      });

  base::test::TestFuture<Thumbnail> future;
  plugin_->ink_module_client_for_testing()->RequestThumbnail(
      /*page_index=*/0, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  Thumbnail thumbnail = future.Take();
  EXPECT_EQ(gfx::Size(108, 140), thumbnail.image_size());
  EXPECT_EQ(1.0f, thumbnail.device_pixel_ratio());
}

TEST_P(PdfViewWebPluginInkTest, AddUpdateDiscardStroke) {
  const PdfInkBrush kBrush(PdfInkBrush::Type::kPen, SK_ColorRED, /*size=*/4.0f);
  constexpr InkStrokeId kStrokeId(1);
  constexpr int kPageIndex = 0;

  EXPECT_CALL(*engine_ptr_, ApplyStroke(kPageIndex, kStrokeId, _));

  const ink::Stroke kStroke(kBrush.ink_brush());
  plugin_->ink_module_client_for_testing()->StrokeAdded(kPageIndex, kStrokeId,
                                                        kStroke);

  EXPECT_CALL(*engine_ptr_, UpdateStrokeActive(kPageIndex, kStrokeId, false));

  plugin_->ink_module_client_for_testing()->UpdateStrokeActive(
      kPageIndex, kStrokeId,
      /*active=*/false);

  EXPECT_CALL(*engine_ptr_, DiscardStroke(kPageIndex, kStrokeId));

  plugin_->ink_module_client_for_testing()->DiscardStroke(kPageIndex,
                                                          kStrokeId);
}

TEST_P(PdfViewWebPluginInkTest, PageIndexFromPoint) {
  ON_CALL(*engine_ptr_, GetPageContentsRect)
      .WillByDefault([](int page_index) -> gfx::Rect {
        // Uniform 80x180 page sizes, with a `kVerticalEmptySpace` gap above
        // every page.
        constexpr int kVerticalEmptySpace = 20;
        constexpr int kHeight = 180;
        int y = kHeight * page_index + kVerticalEmptySpace * (page_index + 1);
        return gfx::Rect(/*x=*/10, /*y=*/y, /*width=*/80, /*height=*/kHeight);
      });

  // Top-left corner of screen.
  constexpr gfx::PointF kScreenTopLeftCorner(0.0f, 0.0f);
  // Top-left corner of first page.
  constexpr gfx::PointF kPage0TopLeftCorner(10.0f, 20.0f);
  // Just outside the top-left corner of first page.
  constexpr gfx::PointF kPage0OutsideTopLeftCorner(10.0f, 19.938f);
  // Bottom-right corner of first page.
  constexpr gfx::PointF kPage0BottomRightCorner(89.999f, 199.0f);
  // Just outside the bottom-right corner of first page.
  constexpr gfx::PointF kPage0OutsideBottomRightCorner(90.0f, 199.0f);
  // Gap between first and second page.
  constexpr gfx::PointF kPage0Page1Gap(50.0f, 201.0f);
  // Top of second page.
  constexpr gfx::PointF kPage1Top(50.0f, 220.0f);
  // Middle of last page.
  constexpr gfx::PointF kPage12Middle(50.0f, 2510.0f);
  // Bottom of last page.
  constexpr gfx::PointF kPage12Bottom(60.0f, 2599.0f);
  // Beyond the last page.
  constexpr gfx::PointF kPageBelowLast(60.0f, 2700.0f);

  // Start with the first 2 pages visible.
  ON_CALL(*engine_ptr_, IsPageVisible)
      .WillByDefault([](int page_index) -> bool {
        return page_index >= 0 && page_index <= 1;
      });

  TestPageIndexFromPoint(kScreenTopLeftCorner, -1, /*expected_visible=*/false);
  TestPageIndexFromPoint(kPage0TopLeftCorner, 0, /*expected_visible=*/true);
  TestPageIndexFromPoint(kPage0OutsideTopLeftCorner, -1,
                         /*expected_visible=*/false);
  TestPageIndexFromPoint(kPage0BottomRightCorner, 0, true);
  TestPageIndexFromPoint(kPage0OutsideBottomRightCorner, -1,
                         /*expected_visible=*/false);
  TestPageIndexFromPoint(kPage0Page1Gap, -1, /*expected_visible=*/false);
  TestPageIndexFromPoint(kPage1Top, 1, /*expected_visible=*/true);
  TestPageIndexFromPoint(kPage12Middle, 12, /*expected_visible=*/false);
  TestPageIndexFromPoint(kPage12Bottom, 12, /*expected_visible=*/false);
  TestPageIndexFromPoint(kPageBelowLast, -1, /*expected_visible=*/false);

  // Change the visible page to the last page.
  ON_CALL(*engine_ptr_, IsPageVisible)
      .WillByDefault([](int page_index) -> bool { return page_index == 12; });

  TestPageIndexFromPoint(kScreenTopLeftCorner, -1, /*expected_visible=*/false);
  TestPageIndexFromPoint(kPage0TopLeftCorner, 0, /*expected_visible=*/false);
  TestPageIndexFromPoint(kPage0OutsideTopLeftCorner, -1,
                         /*expected_visible=*/false);
  TestPageIndexFromPoint(kPage0BottomRightCorner, 0,
                         /*expected_visible=*/false);
  TestPageIndexFromPoint(kPage0OutsideBottomRightCorner, -1,
                         /*expected_visible=*/false);
  TestPageIndexFromPoint(kPage0Page1Gap, -1, /*expected_visible=*/false);
  TestPageIndexFromPoint(kPage1Top, 1, /*expected_visible=*/false);
  TestPageIndexFromPoint(kPage12Middle, 12, /*expected_visible=*/true);
  TestPageIndexFromPoint(kPage12Bottom, 12, /*expected_visible=*/true);
  TestPageIndexFromPoint(kPageBelowLast, -1, /*expected_visible=*/false);
}

TEST_P(PdfViewWebPluginInkTest, IsSelectableTextOrLinkArea) {
  ON_CALL(*engine_ptr_, IsSelectableTextOrLinkArea(gfx::PointF(10.1f, 10.2f)))
      .WillByDefault(Return(true));
  EXPECT_TRUE(
      plugin_->ink_module_client_for_testing()->IsSelectableTextOrLinkArea(
          gfx::PointF(10.1f, 10.2f)));
  EXPECT_FALSE(
      plugin_->ink_module_client_for_testing()->IsSelectableTextOrLinkArea(
          gfx::PointF(20.1f, 20.2f)));
}

TEST_P(PdfViewWebPluginInkTest, OnTextOrLinkAreaClick) {
  EXPECT_CALL(*engine_ptr_, OnTextOrLinkAreaClick(gfx::PointF(1.1f, 2.2f),
                                                  /*click_count=*/2));
  plugin_->ink_module_client_for_testing()->OnTextOrLinkAreaClick(
      gfx::PointF(1.1f, 2.2f), /*click_count=*/2);
}

TEST_P(PdfViewWebPluginInkTest, AnnotationModeSetsFormAndClearsText) {
  EXPECT_CALL(*engine_ptr_, SetFormHighlight(false));
  EXPECT_CALL(*engine_ptr_, ClearTextSelection());

  plugin_->OnMessage(
      CreateSetAnnotationModeMessageForTesting(InkAnnotationMode::kDraw));
  EXPECT_TRUE(plugin_->IsInAnnotationMode());

  EXPECT_CALL(*engine_ptr_, SetFormHighlight(true));

  plugin_->OnMessage(
      CreateSetAnnotationModeMessageForTesting(InkAnnotationMode::kOff));
  EXPECT_FALSE(plugin_->IsInAnnotationMode());
}

TEST_P(PdfViewWebPluginInkTest, DrawInProgressStroke) {
  plugin_->OnMessage(
      CreateSetAnnotationModeMessageForTesting(InkAnnotationMode::kDraw));
  blink::WebMouseEvent start_event =
      CreateLeftClickWebMouseEventAtPosition(kDiagonalStrokeStartPosition);
  blink::WebMouseEvent move_event =
      CreateLeftClickWebMouseMoveEventAtPosition(kDiagonalStrokeEndPosition);
  blink::WebMouseEvent end_event =
      CreateLeftClickWebMouseUpEventAtPosition(kDiagonalStrokeEndPosition);
  TestInProgressDraw(
      /*expected_filename=*/FILE_PATH_LITERAL("diagonal_stroke.png"),
      start_event, {&move_event}, end_event);
}

TEST_P(PdfViewWebPluginInkTest, DrawInProgressStrokeWithTouchWithoutPressure) {
  plugin_->OnMessage(
      CreateSetAnnotationModeMessageForTesting(InkAnnotationMode::kDraw));
  DiagonalStrokeEvents events =
      CreateInputsForDiagonalInProgressStrokeTest(/*is_touch=*/true);
  TestInProgressDraw(
      /*expected_filename=*/FILE_PATH_LITERAL(
          "diagonal_stroke_pen_without_pressure.png"),
      events.start_event, {&events.move_event1, &events.move_event2},
      events.end_event);
}

TEST_P(PdfViewWebPluginInkTest,
       DrawInProgressStrokeWithTouchWithIgnoredPressure) {
  plugin_->OnMessage(
      CreateSetAnnotationModeMessageForTesting(InkAnnotationMode::kDraw));
  DiagonalStrokeEvents events =
      CreateInputsForDiagonalInProgressStrokeTest(/*is_touch=*/true);
  events.start_event.touches[0].force = 0.1f;
  events.move_event1.touches[0].force = 1.0f;
  events.move_event2.touches[0].force = 1.0f;
  events.end_event.touches[0].force = 0.1f;
  TestInProgressDraw(
      /*expected_filename=*/FILE_PATH_LITERAL(
          "diagonal_stroke_pen_without_pressure.png"),
      events.start_event, {&events.move_event1, &events.move_event2},
      events.end_event);
}

TEST_P(PdfViewWebPluginInkTest, DrawInProgressStrokeWithPenWithoutPressure) {
  plugin_->OnMessage(
      CreateSetAnnotationModeMessageForTesting(InkAnnotationMode::kDraw));
  DiagonalStrokeEvents events =
      CreateInputsForDiagonalInProgressStrokeTest(/*is_touch=*/false);
  TestInProgressDraw(
      /*expected_filename=*/FILE_PATH_LITERAL(
          "diagonal_stroke_pen_without_pressure.png"),
      events.start_event, {&events.move_event1, &events.move_event2},
      events.end_event);
}

TEST_P(PdfViewWebPluginInkTest, DrawInProgressStrokeWithPenWithPressure) {
  plugin_->OnMessage(
      CreateSetAnnotationModeMessageForTesting(InkAnnotationMode::kDraw));
  DiagonalStrokeEvents events =
      CreateInputsForDiagonalInProgressStrokeTest(/*is_touch=*/false);
  events.start_event.touches[0].force = 0.1f;
  events.move_event1.touches[0].force = 1.0f;
  events.move_event2.touches[0].force = 1.0f;
  events.end_event.touches[0].force = 0.1f;

  TestInProgressDraw(
      /*expected_filename=*/FILE_PATH_LITERAL(
          "diagonal_stroke_pen_with_pressure.png"),
      events.start_event, {&events.move_event1, &events.move_event2},
      events.end_event);
}

class PdfViewWebPluginInkTextHighlightTest : public PdfViewWebPluginInkTest {
 public:
  static constexpr TestAnnotationBrushMessageParams kLightGreenBrushParams{
      SkColorSetRGB(0x34, 0xA8, 0x53),
      /*size=*/4.5};
  static constexpr gfx::PointF kStartTextPosition{55.0f, 60.0f};
  static constexpr gfx::PointF kEndTextPosition{75.0f, 65.0f};

  // Sets up test expectations for clicking on `kStartTextPosition` and moving
  // to `kEndTextPosition` with a mouse.
  void SetUpMouseDownMoveTextTestExpectations() {
    // The start position and end position are in screen coordinates, while the
    // values passed to and returned from PDFiumEngine are in PDF coordinates.
    // However, in this test fixture, the transforms from canonical coordinates
    // to screen and PDF coordinates are all identity transforms, so the test
    // cases can use the same values regardless of the coordinates system.
    EXPECT_CALL(*engine_ptr_,
                OnTextOrLinkAreaClick(gfx::PointF(5.0f, 60.0f), 1));
    EXPECT_CALL(*engine_ptr_,
                ExtendSelectionByPoint(gfx::PointF(25.0f, 65.0f)));
    PdfInkModuleClient::SelectionRectMap mock_selection_rect_map{
        {0, {PdfRect(5, 60, 25, 65)}}};
    ON_CALL(*engine_ptr_, GetSelectionRectMap())
        .WillByDefault(Return(mock_selection_rect_map));
    ON_CALL(*engine_ptr_, IsSelectableTextOrLinkArea(_))
        .WillByDefault(Return(true));
  }
};

TEST_P(PdfViewWebPluginInkTextHighlightTest, SelectionDoesNotChange) {
  constexpr gfx::Rect kScreenRect(kCanvasSize);
  ON_CALL(*engine_ptr_, GetPageContentsRect).WillByDefault(Return(kScreenRect));
  UpdatePluginGeometry(/*device_scale=*/1.0f, kScreenRect);

  // Enter annotation mode and select the highlighter.
  plugin_->OnMessage(
      CreateSetAnnotationModeMessageForTesting(InkAnnotationMode::kDraw));
  plugin_->OnMessage(CreateSetAnnotationBrushMessageForTesting(
      "highlighter", &kLightGreenBrushParams));

  SetUpMouseDownMoveTextTestExpectations();
  TestSendInputEvent(CreateLeftClickWebMouseEventAtPosition(kStartTextPosition),
                     blink::WebInputEventResult::kHandledApplication);
  TestSendInputEvent(
      CreateLeftClickWebMouseMoveEventAtPosition(kEndTextPosition),
      blink::WebInputEventResult::kHandledApplication);

  EXPECT_CALL(*client_ptr_, TextSelectionChanged(_, _, _)).Times(0);

  plugin_->SetSelectedText("text");

  EXPECT_CALL(pdf_host_, SelectionChanged(_, _, _, _)).Times(0);

  plugin_->SelectionChanged({-10, -20, 30, 40}, {50, 60, 70, 80});
  pdf_receiver_.FlushForTesting();
}

TEST_P(PdfViewWebPluginInkTextHighlightTest, DrawInProgressTextHighlight) {
  // Enter annotation mode and select the highlighter.
  plugin_->OnMessage(
      CreateSetAnnotationModeMessageForTesting(InkAnnotationMode::kDraw));
  plugin_->OnMessage(CreateSetAnnotationBrushMessageForTesting(
      "highlighter", &kLightGreenBrushParams));

  SetUpMouseDownMoveTextTestExpectations();

  static constexpr gfx::PointF kStartPosition{55.0f, 60.0f};
  static constexpr gfx::PointF kEndPosition{75.0f, 65.0f};
  blink::WebMouseEvent start_event =
      CreateLeftClickWebMouseEventAtPosition(kStartPosition);
  blink::WebMouseEvent move_event =
      CreateLeftClickWebMouseMoveEventAtPosition(kEndPosition);
  blink::WebMouseEvent end_event =
      CreateLeftClickWebMouseUpEventAtPosition(kEndPosition);
  TestInProgressDraw(
      /*expected_filename=*/FILE_PATH_LITERAL("text_highlight_stroke.png"),
      start_event, {&move_event}, end_event);
}

class PdfViewWebPluginInk2SaveTest : public PdfViewWebPluginSaveTest {
 private:
  base::test::ScopedFeatureList feature_list_{features::kPdfInk2};
};

TEST_F(PdfViewWebPluginInk2SaveTest, AnnotationInNonEditMode) {
  // Modify the document with an Ink stroke.
  plugin_->ink_module_client_for_testing()->StrokeFinished(/*modified=*/true);

  EXPECT_EQ(blink::WebTextInputType::kWebTextInputTypeNone,
            plugin_->GetPluginTextInputType());

  base::Value expected_response = base::test::ParseJson(R"({
    "type": "saveData",
    "token": "annotation-in-non-edit-mode",
    "fileName": "example.pdf",
    "editModeForTesting": false,
  })");
  AddDataToValue(base::span(TestPDFiumEngine::kSaveData), expected_response);
  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(expected_response)));

  plugin_->OnMessage(ParseMessage(R"({
    "type": "save",
    "saveRequestType": "ANNOTATION",
    "token": "annotation-in-non-edit-mode",
  })"));

  pdf_receiver_.FlushForTesting();
}

TEST_F(PdfViewWebPluginInk2SaveTest, AnnotationInEditMode) {
  // Modify the document with an Ink stroke.
  plugin_->ink_module_client_for_testing()->StrokeFinished(/*modified=*/true);

  plugin_->EnteredEditMode();
  pdf_receiver_.FlushForTesting();

  EXPECT_EQ(blink::WebTextInputType::kWebTextInputTypeNone,
            plugin_->GetPluginTextInputType());

  base::Value expected_response = base::test::ParseJson(R"({
    "type": "saveData",
    "token": "annotation-in-edit-mode",
    "fileName": "example.pdf",
    "editModeForTesting": true,
  })");
  AddDataToValue(base::span(TestPDFiumEngine::kSaveData), expected_response);
  EXPECT_CALL(*client_ptr_, PostMessage(base::test::IsJson(expected_response)));

  plugin_->OnMessage(ParseMessage(R"({
    "type": "save",
    "saveRequestType": "ANNOTATION",
    "token": "annotation-in-edit-mode",
  })"));

  pdf_receiver_.FlushForTesting();
}

using PdfViewWebPluginInkMetricTest = PdfViewWebPluginInkTest;

TEST_P(PdfViewWebPluginInkMetricTest, LoadedWithoutV2InkAnnotations) {
  base::HistogramTester histograms;

  EXPECT_CALL(*engine_ptr_, ContainsV2InkPath(_))
      .WillOnce(Return(PDFLoadedWithV2InkAnnotations::kFalse));
  plugin_->DocumentLoadComplete();

  histograms.ExpectUniqueSample(kPdfLoadedWithV2InkAnnotationsMetric,
                                PDFLoadedWithV2InkAnnotations::kFalse, 1);
}

TEST_P(PdfViewWebPluginInkMetricTest, LoadedWithV2InkAnnotations) {
  base::HistogramTester histograms;

  EXPECT_CALL(*engine_ptr_, ContainsV2InkPath(_))
      .WillOnce(Return(PDFLoadedWithV2InkAnnotations::kTrue));
  plugin_->DocumentLoadComplete();

  histograms.ExpectUniqueSample(kPdfLoadedWithV2InkAnnotationsMetric,
                                PDFLoadedWithV2InkAnnotations::kTrue, 1);
}

TEST_P(PdfViewWebPluginInkMetricTest, LoadedWithV2InkAnnotationsTimeout) {
  base::HistogramTester histograms;
  EXPECT_CALL(*engine_ptr_, ContainsV2InkPath(_))
      .WillOnce(Return(PDFLoadedWithV2InkAnnotations::kUnknown));
  plugin_->DocumentLoadComplete();

  histograms.ExpectUniqueSample(kPdfLoadedWithV2InkAnnotationsMetric,
                                PDFLoadedWithV2InkAnnotations::kUnknown, 1);
}

class PdfViewWebPluginPrintPreviewInkMetricTest
    : public PdfViewWebPluginPrintPreviewTest {
 private:
  base::test::ScopedFeatureList feature_list_{features::kPdfInk2};
};

TEST_F(PdfViewWebPluginPrintPreviewInkMetricTest,
       LoadedWithV2InkAnnotationsDoesNotCountPrintPreview) {
  base::HistogramTester histograms;

  OnMessageWithEngineUpdate(ParseMessage(R"({
    "type": "resetPrintPreviewMode",
    "url": "chrome-untrusted://print/0/0/print.pdf",
    "grayscale": false,
    "pageCount": 1,
  })"));

  EXPECT_CALL(*engine_ptr_, ContainsV2InkPath(_)).Times(0);
  plugin_->DocumentLoadComplete();

  // The V2 ink annotations PDF load metric should not increment for Print
  // Preview.
  histograms.ExpectTotalCount(kPdfLoadedWithV2InkAnnotationsMetric, 0);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PdfViewWebPluginInkTest,
                         testing::ValuesIn(GetAllInkTestVariations()));
INSTANTIATE_TEST_SUITE_P(All,
                         PdfViewWebPluginInkMetricTest,
                         testing::ValuesIn(GetAllInkTestVariations()));
INSTANTIATE_TEST_SUITE_P(
    All,
    PdfViewWebPluginInkTextHighlightTest,
    testing::ValuesIn(GetInkTestVariationsWithTextHighlighting()));
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

class PdfViewWebPluginAnnotationAgentContainerTest
    : public PdfViewWebPluginTest {
 public:
  ~PdfViewWebPluginAnnotationAgentContainerTest() override = default;

  void CreateAgent(blink::mojom::SelectorPtr selector) {
    fake_annotation_agent_host_.reset();
    // IPC disconnection is asynchronous. FlushForTesting() does not work.
    base::RunLoop().RunUntilIdle();

    mojo::PendingReceiver<blink::mojom::AnnotationAgentHost>
        annotation_agent_host_receiver;
    mojo::PendingRemote<blink::mojom::AnnotationAgent> annotation_agent_remote;

    plugin_->CreateAgent(
        annotation_agent_host_receiver.InitWithNewPipeAndPassRemote(),
        annotation_agent_remote.InitWithNewPipeAndPassReceiver(),
        blink::mojom::AnnotationType::kGlic, std::move(selector), std::nullopt);
    fake_annotation_agent_host_ = std::make_unique<FakeAnnotationAgentHost>(
        std::move(annotation_agent_host_receiver),
        std::move(annotation_agent_remote));
  }

 protected:
  std::unique_ptr<FakeAnnotationAgentHost> fake_annotation_agent_host_;
};

TEST_F(PdfViewWebPluginAnnotationAgentContainerTest, TextFragmentFound) {
  EXPECT_CALL(*engine_ptr_,
              FindAndHighlightTextFragments(ElementsAre("does_not_matter")))
      .WillOnce(Return(true));
  EXPECT_CALL(*engine_ptr_, ScrollToFirstTextFragment).Times(0);
  CreateAgent(blink::mojom::Selector::NewSerializedSelector("does_not_matter"));
  EXPECT_EQ(fake_annotation_agent_host_->WaitForAttachmentResult(),
            blink::mojom::AttachmentResult::kSuccess);
}

TEST_F(PdfViewWebPluginAnnotationAgentContainerTest, TextFragmentNotFound) {
  EXPECT_CALL(*engine_ptr_,
              FindAndHighlightTextFragments(ElementsAre("does_not_matter")))
      .WillOnce(Return(false));
  EXPECT_CALL(*engine_ptr_, ScrollToFirstTextFragment).Times(0);
  CreateAgent(blink::mojom::Selector::NewSerializedSelector("does_not_matter"));
  EXPECT_EQ(fake_annotation_agent_host_->WaitForAttachmentResult(),
            blink::mojom::AttachmentResult::kSelectorNotMatched);
}

TEST_F(PdfViewWebPluginAnnotationAgentContainerTest, EmptySelector) {
  EXPECT_CALL(*engine_ptr_, FindAndHighlightTextFragments).Times(0);
  EXPECT_CALL(*engine_ptr_, ScrollToFirstTextFragment).Times(0);
  CreateAgent(blink::mojom::Selector::NewSerializedSelector(""));
}

TEST_F(PdfViewWebPluginAnnotationAgentContainerTest, NodeSelector) {
  EXPECT_CALL(*engine_ptr_, FindAndHighlightTextFragments).Times(0);
  EXPECT_CALL(*engine_ptr_, ScrollToFirstTextFragment).Times(0);
  CreateAgent(blink::mojom::Selector::NewNodeId(1));
}

TEST_F(PdfViewWebPluginAnnotationAgentContainerTest,
       ResetPreviousTextFragment) {
  EXPECT_CALL(*engine_ptr_, FindAndHighlightTextFragments)
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*engine_ptr_, ScrollToFirstTextFragment).Times(0);
  // Every agent will eventually disconnect and trigger `RemoveTextFragments()`.
  EXPECT_CALL(*engine_ptr_, RemoveTextFragments).Times(2);
  CreateAgent(blink::mojom::Selector::NewSerializedSelector("does_not_matter"));
  CreateAgent(blink::mojom::Selector::NewSerializedSelector("does_not_matter"));
}

TEST_F(PdfViewWebPluginAnnotationAgentContainerTest, ScrollIntoView) {
  EXPECT_CALL(*engine_ptr_, FindAndHighlightTextFragments)
      .WillOnce(Return(true));
  EXPECT_CALL(*engine_ptr_,
              ScrollToFirstTextFragment(/*force_smooth_scroll=*/true));
  CreateAgent(blink::mojom::Selector::NewSerializedSelector("does_not_matter"));
  fake_annotation_agent_host_->ScrollIntoView();
}

TEST_F(PdfViewWebPluginAnnotationAgentContainerTest, ConsecutiveQueries) {
  EXPECT_CALL(*engine_ptr_, FindAndHighlightTextFragments)
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*engine_ptr_, RemoveTextFragments).Times(2);
  {
    EXPECT_CALL(*engine_ptr_,
                ScrollToFirstTextFragment(/*force_smooth_scroll=*/true));
    CreateAgent(
        blink::mojom::Selector::NewSerializedSelector("does_not_matter"));
    fake_annotation_agent_host_->ScrollIntoView();
  }
  {
    EXPECT_CALL(*engine_ptr_,
                ScrollToFirstTextFragment(/*force_smooth_scroll=*/true));
    CreateAgent(
        blink::mojom::Selector::NewSerializedSelector("does_not_matter"));
    fake_annotation_agent_host_->ScrollIntoView();
  }
}

TEST_F(PdfViewWebPluginAnnotationAgentContainerTest,
       CannotScrollAfterInvalidation) {
  EXPECT_CALL(*engine_ptr_, FindAndHighlightTextFragments)
      .WillOnce(Return(true));
  EXPECT_CALL(*engine_ptr_, ScrollToFirstTextFragment).Times(0);
  EXPECT_CALL(*engine_ptr_, RemoveTextFragments);

  CreateAgent(blink::mojom::Selector::NewSerializedSelector("does_not_matter"));
  plugin_->OnNewTextFragmentsSearchStarted();
  fake_annotation_agent_host_->ScrollIntoView();
}

}  // namespace chrome_pdf
