// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_android.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/values.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"
#include "third_party/icu/source/i18n/unicode/ulocdata.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "printing/printing_jni_headers/PrintingContext_jni.h"

using base::android::ScopedJavaLocalRef;

namespace printing {

namespace {

// Sets the page sizes for a `PrintSettings` object.  `width` and `height`
// arguments should be in device units.
void SetSizes(PrintSettings* settings, int dpi, int width, int height) {
  gfx::Size physical_size_device_units(width, height);
  // Assume full page is printable for now.
  gfx::Rect printable_area_device_units(0, 0, width, height);

  settings->set_dpi(dpi);
  settings->SetPrinterPrintableArea(physical_size_device_units,
                                    printable_area_device_units, false);
}

}  // namespace

// static
std::unique_ptr<PrintingContext> PrintingContext::CreateImpl(
    Delegate* delegate,
    OutOfProcessBehavior out_of_process_behavior) {
  DCHECK_EQ(out_of_process_behavior, OutOfProcessBehavior::kDisabled);
  return std::make_unique<PrintingContextAndroid>(delegate);
}

void PrintingContextAndroid::PdfWritingDone(int page_count,
                                            ui::WindowAndroid* window) {
  if (!window) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PrintingContext_pdfWritingDone(env, page_count, window->GetJavaObject());
}

// static
void PrintingContextAndroid::SetPendingPrint(
    ui::WindowAndroid* window,
    const ScopedJavaLocalRef<jobject>& printable,
    int render_process_id,
    int render_frame_id) {
  if (!window) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PrintingContext_setPendingPrint(env, window->GetJavaObject(), printable,
                                       render_process_id, render_frame_id);
}

PrintingContextAndroid::PrintingContextAndroid(Delegate* delegate)
    : PrintingContext(delegate, OutOfProcessBehavior::kDisabled) {
  // The constructor is run in the IO thread.
}

PrintingContextAndroid::~PrintingContextAndroid() {}

void PrintingContextAndroid::AskUserForSettings(
    int max_pages,
    bool has_selection,
    bool is_scripted,
    PrintSettingsCallback callback) {
  // This method is always run in the UI thread.
  callback_ = std::move(callback);

  JNIEnv* env = base::android::AttachCurrentThread();
  if (j_printing_context_.is_null()) {
    auto* parent_view =
        static_cast<ui::ViewAndroid*>(delegate_->GetParentView());
    ui::WindowAndroid* window =
        parent_view ? parent_view->GetWindowAndroid() : nullptr;
    if (!window) {
      std::move(callback_).Run(mojom::ResultCode::kCanceled);
      return;
    }
    j_printing_context_.Reset(Java_PrintingContext_create(
        env, reinterpret_cast<intptr_t>(this), window->GetJavaObject()));
  }

  if (is_scripted) {
    Java_PrintingContext_showPrintDialog(env, j_printing_context_);
  } else {
    // The non-scripted path is served by PrintViewManagerBasic, which answers
    // ScriptedPrint() directly from the settings it received at onWrite().
    // Reaching here means those settings are already gone (e.g. the print
    // session was torn down while the renderer was still printing), so cancel
    // instead of crashing the browser on a renderer-driven race.
    std::move(callback_).Run(mojom::ResultCode::kCanceled);
  }
}

void PrintingContextAndroid::ShowSystemDialogDone(JNIEnv* env) {
  DCHECK(callback_);
  // Settings are not updated, callback is called only to unblock javascript.
  std::move(callback_).Run(mojom::ResultCode::kCanceled);
}

mojom::ResultCode PrintingContextAndroid::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  ResetSettings();
  settings_->set_dpi(kDefaultPdfDpi);
  gfx::Size physical_size = GetPdfPaperSizeDeviceUnits();
  SetSizes(settings_.get(), kDefaultPdfDpi, physical_size.width(),
           physical_size.height());
  return mojom::ResultCode::kSuccess;
}

gfx::Size PrintingContextAndroid::GetPdfPaperSizeDeviceUnits() {
  // NOTE: This implementation is the same as in PrintingContextNoSystemDialog.
  int32_t width = 0;
  int32_t height = 0;
  UErrorCode error = U_ZERO_ERROR;
  ulocdata_getPaperSize(delegate_->GetAppLocale().c_str(), &height, &width,
                        &error);
  if (error > U_ZERO_ERROR) {
    // If the call failed, assume a paper size of 8.5 x 11 inches.
    LOG(WARNING) << "ulocdata_getPaperSize failed, using 8.5 x 11, error: "
                 << error;
    width =
        static_cast<int>(kLetterWidthInch * settings_->device_units_per_inch());
    height = static_cast<int>(kLetterHeightInch *
                              settings_->device_units_per_inch());
  } else {
    // ulocdata_getPaperSize returns the width and height in mm.
    // Convert this to pixels based on the dpi.
    float multiplier = settings_->device_units_per_inch() / kMicronsPerMil;
    width *= multiplier;
    height *= multiplier;
  }
  return gfx::Size(width, height);
}

mojom::ResultCode PrintingContextAndroid::UpdatePrinterSettings(
    const PrinterSettings& printer_settings) {
  DCHECK(!printer_settings.show_system_dialog);
  DCHECK(!in_print_job_);

  // Intentional No-op.

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintingContextAndroid::NewDocument(
    const std::u16string& document_name) {
  DCHECK(!in_print_job_);
  in_print_job_ = true;

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintingContextAndroid::PrintDocument(
    const MetafilePlayer& metafile,
    const PrintSettings& settings,
    uint32_t num_pages) {
  // On Android, PDF generation is handled directly by PrintViewManagerBasic
  // writing asynchronously to the destination file descriptor via ThreadPool,
  // bypassing the legacy PrintJob / PrintingContext::PrintDocument pipeline.
  return mojom::ResultCode::kFailed;
}

mojom::ResultCode PrintingContextAndroid::DocumentDone() {
  if (abort_printing_)
    return mojom::ResultCode::kCanceled;
  DCHECK(in_print_job_);

  ResetSettings();
  return mojom::ResultCode::kSuccess;
}

void PrintingContextAndroid::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
}

void PrintingContextAndroid::ReleaseContext() {}

printing::NativeDrawingContext PrintingContextAndroid::context() const {
  // Intentional No-op.
  return nullptr;
}

}  // namespace printing

DEFINE_JNI(PrintingContext)
