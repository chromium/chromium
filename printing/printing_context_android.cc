// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_android.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check_op.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "printing/metafile.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"
#include "third_party/icu/source/i18n/unicode/ulocdata.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "printing/printing_jni_headers/PrintingContext_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
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

void GetPageRanges(JNIEnv* env,
                   const JavaRef<jintArray>& int_arr,
                   PageRanges* range_vector) {
  std::vector<int> pages;
  base::android::JavaIntArrayToIntVector(env, int_arr, &pages);
  for (int page : pages) {
    PageRange range;
    range.from = page;
    range.to = page;
    range_vector->push_back(range);
  }
}

}  // namespace

// static
std::unique_ptr<PrintingContext> PrintingContext::CreateImpl(
    Delegate* delegate,
    ProcessBehavior process_behavior) {
  DCHECK_EQ(process_behavior, ProcessBehavior::kOopDisabled);
  return std::make_unique<PrintingContextAndroid>(delegate);
}

// static
void PrintingContextAndroid::PdfWritingDone(int page_count) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PrintingContext_pdfWritingDone(env, page_count);
}

// static
void PrintingContextAndroid::SetPendingPrint(
    ui::WindowAndroid* window,
    const ScopedJavaLocalRef<jobject>& printable,
    int render_process_id,
    int render_frame_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PrintingContext_setPendingPrint(env, window->GetJavaObject(), printable,
                                       render_process_id, render_frame_id);
}

PrintingContextAndroid::PrintingContextAndroid(Delegate* delegate)
    : PrintingContext(delegate, ProcessBehavior::kOopDisabled) {
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
    j_printing_context_.Reset(
        Java_PrintingContext_create(env, reinterpret_cast<intptr_t>(this)));
  }

  if (is_scripted) {
    Java_PrintingContext_showPrintDialog(env, j_printing_context_);
  } else {
    Java_PrintingContext_askUserForSettings(env, j_printing_context_,
                                            max_pages);
  }
}

void PrintingContextAndroid::AskUserForSettingsReply(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean success) {
  DCHECK(callback_);
  if (!success) {
    // TODO(cimamoglu): Differentiate between `kFailed` And `kCancel`.
    std::move(callback_).Run(mojom::ResultCode::kFailed);
    return;
  }

  // We use device name variable to store the file descriptor.  This is hacky
  // but necessary. Since device name is not necessary for the upstream
  // printing code for Android, this is harmless.
  // TODO(thestig): See if the call to set_device_name() can be removed.
  fd_ = Java_PrintingContext_getFileDescriptor(env, j_printing_context_);
  DCHECK(is_file_descriptor_valid());
  settings_->set_device_name(base::NumberToString16(fd_));

  ScopedJavaLocalRef<jintArray> intArr =
      Java_PrintingContext_getPages(env, j_printing_context_);
  if (!intArr.is_null()) {
    PageRanges range_vector;
    GetPageRanges(env, intArr, &range_vector);
    settings_->set_ranges(range_vector);
  }

  int dpi = Java_PrintingContext_getDpi(env, j_printing_context_);
  int width = Java_PrintingContext_getWidth(env, j_printing_context_);
  int height = Java_PrintingContext_getHeight(env, j_printing_context_);
  width = ConvertUnit(width, kMilsPerInch, dpi);
  height = ConvertUnit(height, kMilsPerInch, dpi);
  SetSizes(settings_.get(), dpi, width, height);

  std::move(callback_).Run(mojom::ResultCode::kSuccess);
}

void PrintingContextAndroid::ShowSystemDialogDone(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
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
  if (abort_printing_)
    return mojom::ResultCode::kCanceled;
  DCHECK(in_print_job_);
  DCHECK(is_file_descriptor_valid());

  return metafile.SaveToFileDescriptor(fd_) ? mojom::ResultCode::kSuccess
                                            : mojom::ResultCode::kFailed;
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

void PrintingContextAndroid::ReleaseContext() {
  // Intentional No-op.
}

printing::NativeDrawingContext PrintingContextAndroid::context() const {
  // Intentional No-op.
  return nullptr;
}

}  // namespace printing
