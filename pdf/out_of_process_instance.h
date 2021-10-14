// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_OUT_OF_PROCESS_INSTANCE_H_
#define PDF_OUT_OF_PROCESS_INSTANCE_H_

#include <stdint.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "pdf/pdf_view_plugin_base.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "ppapi/cpp/dev/printing_dev.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/private/find_private.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace pp {
class Size;
class TextInput_Dev;
}  // namespace pp

namespace chrome_pdf {

class Graphics;
class UrlLoader;

class OutOfProcessInstance : public PdfViewPluginBase,
                             public pp::Instance,
                             public pp::Find_Private,
                             public pp::Printing_Dev {
 public:
  explicit OutOfProcessInstance(PP_Instance instance);
  OutOfProcessInstance(const OutOfProcessInstance&) = delete;
  OutOfProcessInstance& operator=(const OutOfProcessInstance&) = delete;
  ~OutOfProcessInstance() override;

  // pp::Instance:
  bool Init(uint32_t argc, const char* argn[], const char* argv[]) override;
  void HandleMessage(const pp::Var& message) override;
  bool HandleInputEvent(const pp::InputEvent& event) override;
  void DidChangeView(const pp::View& view) override;
  void DidChangeFocus(bool has_focus) override;

  // pp::Find_Private:
  bool StartFind(const std::string& text, bool case_sensitive) override;
  void SelectFindResult(bool forward) override;
  void StopFind() override;

  // pp::Printing_Dev:
  uint32_t QuerySupportedPrintOutputFormats() override;
  int32_t PrintBegin(const PP_PrintSettings_Dev& print_settings) override;
  pp::Resource PrintPages(const PP_PrintPageNumberRange_Dev* page_ranges,
                          uint32_t page_range_count) override;
  void PrintEnd() override;
  bool IsPrintScalingDisabled() override;

  // pp::Private:
  pp::Var GetLinkAtPosition(const pp::Point& point);
  void GetPrintPresetOptionsFromDocument(PP_PdfPrintPresetOptions_Dev* options);
  void SetCaretPosition(const pp::FloatPoint& position);
  void MoveRangeSelectionExtent(const pp::FloatPoint& extent);
  void SetSelectionBounds(const pp::FloatPoint& base,
                          const pp::FloatPoint& extent);
  bool CanEditText();
  bool HasEditableText();
  void ReplaceSelection(const std::string& text);
  void SelectAll();
  bool CanUndo();
  bool CanRedo();
  void Undo();
  void Redo();
  int32_t PdfPrintBegin(const PP_PrintSettings_Dev* print_settings,
                        const PP_PdfPrintSettings_Dev* pdf_print_settings);

  void FlushCallback(int32_t result);

  // PdfViewPluginBase:
  void UpdateCursor(ui::mojom::CursorType new_cursor_type) override;
  void NotifySelectedFindResultChanged(int current_find_index) override;
  void CaretChanged(const gfx::Rect& caret_rect) override;
  void Alert(const std::string& message) override;
  bool Confirm(const std::string& message) override;
  std::string Prompt(const std::string& question,
                     const std::string& default_answer) override;
  std::vector<SearchStringResult> SearchString(const char16_t* string,
                                               const char16_t* term,
                                               bool case_sensitive) override;
  void SetLastPluginInstance() override;
  void SetSelectedText(const std::string& selected_text) override;
  bool IsValidLink(const std::string& url) override;
  std::unique_ptr<Graphics> CreatePaintGraphics(const gfx::Size& size) override;
  bool BindPaintGraphics(Graphics& graphics) override;
  void ScheduleTaskOnMainThread(const base::Location& from_here,
                                ResultCallback callback,
                                int32_t result,
                                base::TimeDelta delay) override;

  // Helper functions for implementing PPP_PDF.
  void RotateClockwise();
  void RotateCounterclockwise();

 protected:
  // PdfViewPluginBase:
  base::WeakPtr<PdfViewPluginBase> GetWeakPtr() override;
  std::unique_ptr<UrlLoader> CreateUrlLoaderInternal() override;
  std::string RewriteRequestUrl(base::StringPiece url) const override;
  void SendMessage(base::Value message) override;
  void SaveAs() override;
  void InitImageData(const gfx::Size& size) override;
  Image GetPluginImageData() const override;
  void SetFormFieldInFocus(bool in_focus) override;
  void SetAccessibilityDocInfo(const AccessibilityDocInfo& doc_info) override;
  void SetAccessibilityPageInfo(AccessibilityPageInfo page_info,
                                std::vector<AccessibilityTextRunInfo> text_runs,
                                std::vector<AccessibilityCharInfo> chars,
                                AccessibilityPageObjects page_objects) override;
  void SetAccessibilityViewportInfo(
      const AccessibilityViewportInfo& viewport_info) override;
  void NotifyFindResultsChanged(int total, bool final_result) override;
  void NotifyFindTickmarks(const std::vector<gfx::Rect>& tickmarks) override;
  void SetContentRestrictions(int content_restrictions) override;
  void SetPluginCanSave(bool can_save) override;
  void PluginDidStartLoading() override;
  void PluginDidStopLoading() override;
  void InvokePrintDialog() override;
  void NotifyLinkUnderCursor() override;
  void NotifySelectionChanged(const gfx::PointF& left,
                              int left_height,
                              const gfx::PointF& right,
                              int right_height) override;
  void NotifyUnsupportedFeature() override;
  void UserMetricsRecordAction(const std::string& action) override;

 private:
  bool CanSaveEdits() const;

  // The Pepper image data that is in sync with mutable_image_data().
  pp::ImageData pepper_image_data_;

  // Used for submitting forms.
  std::unique_ptr<UrlLoader> form_loader_;

  // Used to signal the browser about focus changes to trigger the OSK.
  // TODO(abodenha@chromium.org) Implement full IME support in the plugin.
  // http://crbug.com/132565
  std::unique_ptr<pp::TextInput_Dev> text_input_;

  base::WeakPtrFactory<OutOfProcessInstance> weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_OUT_OF_PROCESS_INSTANCE_H_
