// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_ENGINE_CLIENT_H_
#define PDF_PDFIUM_PDFIUM_ENGINE_CLIENT_H_

#include <stdint.h>
#include <uchar.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "pdf/buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "v8/include/v8-forward.h"

namespace gfx {
class Vector2d;
}  // namespace gfx

namespace chrome_pdf {

class DocumentLayout;
class UrlLoader;

// The interface that's provided to the PDFium rendering engine.
class PDFiumEngineClient {
 public:
  enum class FocusFieldType {
    // Focus is not on any form field.
    kNoFocus,
    // Focus is on a form text field or form combobox text field.
    kText,
    // Focus is on a non-text field.
    kNonText,
  };

  virtual ~PDFiumEngineClient() = default;

  // Proposes a document layout to the client. For the proposed layout to
  // become effective, the client must call PDFiumEngine::ApplyDocumentLayout()
  // with the new layout options (although this call can be asynchronous).
  virtual void ProposeDocumentLayout(const DocumentLayout& layout) = 0;

  // Informs the client that the given rect needs to be repainted.
  virtual void Invalidate(const gfx::Rect& rect) {}

  // Informs the client to scroll the plugin area by the given offset.
  virtual void DidScroll(const gfx::Vector2d& offset) {}

  // Scroll the horizontal/vertical scrollbars to a given position.
  // Values are in screen coordinates, where 0 is the top/left of the document
  // and a positive value is the distance in pixels from that line.
  virtual void ScrollToX(int x_screen_coords) {}
  virtual void ScrollToY(int y_screen_coords) {}

  // Scroll by a given delta relative to the current position.
  virtual void ScrollBy(const gfx::Vector2d& delta) {}

  // Scroll to zero-based `page`.
  virtual void ScrollToPage(int page) {}

  // Navigate to the given url.
  virtual void NavigateTo(const std::string& url,
                          WindowOpenDisposition disposition) {}

  // Navigate to the given destination. Zero-based `page` index. `x`, `y` and
  // `zoom` are optional and can be nullptr.
  virtual void NavigateToDestination(int page,
                                     const float* x,
                                     const float* y,
                                     const float* zoom) {}

  // Updates the cursor.
  virtual void UpdateCursor(ui::mojom::CursorType new_cursor_type) {}

  // Updates the tick marks in the vertical scrollbar.
  virtual void UpdateTickMarks(const std::vector<gfx::Rect>& tickmarks) {}

  // Updates the number of find results for the current search term.  If
  // there are no matches 0 should be passed in.  Only when the plugin has
  // finished searching should it pass in the final count with `final_result`
  // set to true.
  virtual void NotifyNumberOfFindResultsChanged(int total, bool final_result) {}

  // Updates the index of the currently selected search item. Set
  // `final_result` to true only when there is no subsequent
  // `NotifyNumberOfFindResultsChanged()` call.
  virtual void NotifySelectedFindResultChanged(int current_find_index,
                                               bool final_result) {}

  virtual void NotifyTouchSelectionOccurred() {}

  // Prompts the user for a password to open this document. The callback is
  // called when the password is retrieved.
  virtual void GetDocumentPassword(
      base::OnceCallback<void(const std::string&)> callback) {}

  // Play a "beeping" sound.
  virtual void Beep() {}

  // Puts up an alert with the given message.
  virtual void Alert(const std::string& message) {}

  // Puts up a confirm with the given message, and returns true if the user
  // presses OK, or false if they press cancel.
  virtual bool Confirm(const std::string& message) = 0;

  // Puts up a prompt with the given message and default answer and returns
  // the answer.
  virtual std::string Prompt(const std::string& question,
                             const std::string& default_answer) = 0;

  // Returns the url of the pdf.
  virtual std::string GetURL() = 0;

  // Send an email.
  virtual void Email(const std::string& to,
                     const std::string& cc,
                     const std::string& bcc,
                     const std::string& subject,
                     const std::string& body) {}

  // Put up the print dialog.
  virtual void Print() {}

  // Submit the data using HTTP POST.
  virtual void SubmitForm(const std::string& url,
                          const void* data,
                          int length) {}

  // Creates and returns new URL loader for partial document requests.
  virtual std::unique_ptr<UrlLoader> CreateUrlLoader() = 0;

  // Returns the current V8 isolate, if any.
  virtual v8::Isolate* GetIsolate() = 0;

  // Searches the given string for "term" and returns the results.  Unicode-
  // aware.
  struct SearchStringResult {
    int start_index;
    int length;
  };
  virtual std::vector<SearchStringResult> SearchString(const char16_t* string,
                                                       const char16_t* term,
                                                       bool case_sensitive) = 0;

  // Notifies the client that the document has finished loading.
  virtual void DocumentLoadComplete() {}

  // Notifies the client that the document has failed to load.
  virtual void DocumentLoadFailed() {}

  // Notifies that an unsupported feature in the PDF was encountered.
  virtual void DocumentHasUnsupportedFeature(const std::string& feature) {}

  // Notifies the client about document load progress.
  virtual void DocumentLoadProgress(uint32_t available, uint32_t doc_size) {}

  // Notifies the client about focus changes for form fields.
  virtual void FormFieldFocusChange(FocusFieldType type) {}

  // Returns true if the plugin has been opened within print preview.
  virtual bool IsPrintPreview() const = 0;

  // Get the background color of the PDF.
  virtual SkColor GetBackgroundColor() const = 0;

  // Sets selection status.
  virtual void SelectionChanged(const gfx::Rect& left, const gfx::Rect& right) {
  }

  // The caret position in the editable form (if applicable) changed.
  virtual void CaretChanged(const gfx::Rect& caret_rect) {}

  // Notifies the client that the PDF has been edited.
  virtual void EnteredEditMode() {}

  // Notifies the client about focus changes for the document.
  virtual void DocumentFocusChanged(bool document_has_focus) {}

  // Sets selected text.
  virtual void SetSelectedText(const std::string& selected_text) = 0;

  // Sets the link under cursor.
  virtual void SetLinkUnderCursor(const std::string& link_under_cursor) = 0;

  // If the link cannot be converted to JS payload struct, then it is not
  // possible to pass it to JS. In this case, ignore the link like other PDF
  // viewers.
  // See https://crbug.com/312882 for an example.
  virtual bool IsValidLink(const std::string& url) = 0;

#if BUILDFLAG(ENABLE_PDF_INK2)
  // Returns true if the client is in annotation mode.
  virtual bool IsInAnnotationMode() const = 0;
#endif  // BUILDFLAG(ENABLE_PDF_INK2)
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_ENGINE_CLIENT_H_
