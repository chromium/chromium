// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/text_detection_impl_win.h"

#include <windows.foundation.collections.h>
#include <windows.globalization.h>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "base/win/scoped_hstring.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/shape_detection/detection_utils_win.h"
#include "services/shape_detection/text_detection_impl.h"
#include "ui/gfx/geometry/rect_f.h"

namespace shape_detection {

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Globalization::ILanguageFactory;
using ABI::Windows::Graphics::Imaging::ISoftwareBitmap;
using ABI::Windows::Graphics::Imaging::ISoftwareBitmapStatics;
using ABI::Windows::Media::Ocr::IOcrEngine;
using ABI::Windows::Media::Ocr::IOcrEngineStatics;
using ABI::Windows::Media::Ocr::IOcrLine;
using ABI::Windows::Media::Ocr::IOcrResult;
using ABI::Windows::Media::Ocr::IOcrWord;
using ABI::Windows::Media::Ocr::OcrLine;
using ABI::Windows::Media::Ocr::OcrResult;
using ABI::Windows::Media::Ocr::OcrWord;
using base::win::GetActivationFactory;
using base::win::ScopedHString;
using Microsoft::WRL::ComPtr;

// static
void TextDetectionImpl::Create(
    mojo::PendingReceiver<mojom::TextDetection> receiver) {
  // Text Detection specification only supports Latin-1 text as documented in
  // https://wicg.github.io/shape-detection-api/text.html#text-detection-api.
  // TODO(junwei.fu): https://crbug.com/794097 consider supporting other Latin
  // script language.
  ScopedHString language_hstring = ScopedHString::Create("en");
  if (!language_hstring.is_valid())
    return;

  ComPtr<ILanguageFactory> language_factory;
  HRESULT hr =
      GetActivationFactory<ILanguageFactory,
                           RuntimeClass_Windows_Globalization_Language>(
          &language_factory);
  if (FAILED(hr)) {
    DLOG(ERROR) << "ILanguage factory failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  ComPtr<ABI::Windows::Globalization::ILanguage> language;
  hr = language_factory->CreateLanguage(language_hstring.get(), &language);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Create language failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  ComPtr<IOcrEngineStatics> engine_factory;
  hr = GetActivationFactory<IOcrEngineStatics,
                            RuntimeClass_Windows_Media_Ocr_OcrEngine>(
      &engine_factory);
  if (FAILED(hr)) {
    DLOG(ERROR) << "IOcrEngineStatics factory failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  boolean is_supported = false;
  hr = engine_factory->IsLanguageSupported(language.Get(), &is_supported);
  if (FAILED(hr) || !is_supported)
    return;

  ComPtr<IOcrEngine> ocr_engine;
  hr = engine_factory->TryCreateFromLanguage(language.Get(), &ocr_engine);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Create engine failed from language: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  ComPtr<ISoftwareBitmapStatics> bitmap_factory;
  hr = GetActivationFactory<
      ISoftwareBitmapStatics,
      RuntimeClass_Windows_Graphics_Imaging_SoftwareBitmap>(&bitmap_factory);
  if (FAILED(hr)) {
    DLOG(ERROR) << "ISoftwareBitmapStatics factory failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  auto impl = std::make_unique<TextDetectionImplWin>(std::move(ocr_engine),
                                                     std::move(bitmap_factory));
  auto* impl_ptr = impl.get();
  impl_ptr->SetReceiver(
      mojo::MakeSelfOwnedReceiver(std::move(impl), std::move(receiver)));
}

TextDetectionImplWin::TextDetectionImplWin(
    ComPtr<IOcrEngine> ocr_engine,
    ComPtr<ISoftwareBitmapStatics> bitmap_factory)
    : ocr_engine_(std::move(ocr_engine)),
      bitmap_factory_(std::move(bitmap_factory)) {
  DCHECK(ocr_engine_);
  DCHECK(bitmap_factory_);
}

TextDetectionImplWin::~TextDetectionImplWin() = default;

void TextDetectionImplWin::Detect(const SkBitmap& bitmap,
                                  DetectCallback callback) {
  if (FAILED(BeginDetect(bitmap))) {
    // No detection taking place; run |callback| with an empty array of results.
    std::move(callback).Run(std::vector<mojom::TextDetectionResultPtr>());
    return;
  }
  // Hold on the callback until AsyncOperation completes.
  recognize_text_callback_ = std::move(callback);
  // This prevents the Detect function from being called before the
  // AsyncOperation completes.
  receiver_->PauseIncomingMethodCallProcessing();
}

HRESULT TextDetectionImplWin::BeginDetect(const SkBitmap& bitmap) {
  ComPtr<ISoftwareBitmap> win_bitmap =
      CreateWinBitmapFromSkBitmap(bitmap, bitmap_factory_.Get());
  if (!win_bitmap)
    return E_FAIL;

  // Recognize text asynchronously.
  ComPtr<IAsyncOperation<OcrResult*>> async_op;
  const HRESULT hr = ocr_engine_->RecognizeAsync(win_bitmap.Get(), &async_op);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Recognize text asynchronously failed: "
                << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  // Use WeakPtr to bind the callback so that the once callback will not be run
  // if this object has been already destroyed. |win_bitmap| needs to be kept
  // alive until OnTextDetected().
  return base::win::PostAsyncResults(
      std::move(async_op),
      base::BindOnce(&TextDetectionImplWin::OnTextDetected,
                     weak_factory_.GetWeakPtr(), std::move(win_bitmap)));
}

std::vector<mojom::TextDetectionResultPtr>
TextDetectionImplWin::BuildTextDetectionResult(ComPtr<IOcrResult> ocr_result) {
  std::vector<mojom::TextDetectionResultPtr> results;
  if (!ocr_result)
    return results;

  ComPtr<IVectorView<OcrLine*>> ocr_lines;
  HRESULT hr = ocr_result->get_Lines(&ocr_lines);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Get Lines failed: " << logging::SystemErrorCodeToString(hr);
    return results;
  }

  uint32_t count;
  hr = ocr_lines->get_Size(&count);
  if (FAILED(hr)) {
    DLOG(ERROR) << "get_Size failed: " << logging::SystemErrorCodeToString(hr);
    return results;
  }

  results.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    ComPtr<IOcrLine> line;
    hr = ocr_lines->GetAt(i, &line);
    if (FAILED(hr))
      break;

    HSTRING text;
    hr = line->get_Text(&text);
    if (FAILED(hr))
      break;

    // Gets bounding box with the words detected in the current line of Text.
    ComPtr<IVectorView<OcrWord*>> ocr_words;
    hr = line->get_Words(&ocr_words);
    if (FAILED(hr))
      break;

    uint32_t words_count;
    hr = ocr_words->get_Size(&words_count);
    if (FAILED(hr))
      break;

    auto result = shape_detection::mojom::TextDetectionResult::New();
    for (uint32_t word_num = 0; word_num < words_count; ++word_num) {
      ComPtr<IOcrWord> word;
      hr = ocr_words->GetAt(word_num, &word);
      if (FAILED(hr))
        break;

      ABI::Windows::Foundation::Rect bounds;
      hr = word->get_BoundingRect(&bounds);
      if (FAILED(hr))
        break;

      result->bounding_box = gfx::UnionRects(
          result->bounding_box,
          gfx::RectF(bounds.X, bounds.Y, bounds.Width, bounds.Height));
    }

    result->raw_value = ScopedHString(text).GetAsUTF8();
    results.push_back(std::move(result));
  }
  return results;
}

// |win_bitmap| is passed here so that it is kept alive until the AsyncOperation
// completes because RecognizeAsync does not hold a reference.
void TextDetectionImplWin::OnTextDetected(
    ComPtr<ISoftwareBitmap> /* win_bitmap */,
    ComPtr<IOcrResult> ocr_result) {
  std::move(recognize_text_callback_)
      .Run(BuildTextDetectionResult(std::move(ocr_result)));
  receiver_->ResumeIncomingMethodCallProcessing();
}

}  // namespace shape_detection
