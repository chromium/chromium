/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/text/collator.h"

#include <stdlib.h>
#include <string.h>
#include <unicode/ucol.h>
#include <memory>
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace WTF {

static UCollator* g_cached_collator;
static char g_cached_equivalent_locale[Collator::kUlocFullnameCapacity];
static Mutex& CachedCollatorMutex() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, ());
  return mutex;
}

Collator::Collator(const char* locale)
    : collator_(nullptr),
      locale_(locale ? strdup(locale) : nullptr),
      lower_first_(false) {
  SetEquivalentLocale(locale_, equivalent_locale_);
}

std::unique_ptr<Collator> Collator::UserDefault() {
  return std::make_unique<Collator>(nullptr);
}

Collator::~Collator() {
  ReleaseCollator();
  free(locale_);
}

void Collator::SetOrderLowerFirst(bool lower_first) {
  lower_first_ = lower_first;
}

Collator::Result Collator::Collate(const UChar* lhs,
                                   uint32_t lhs_length,
                                   const UChar* rhs,
                                   uint32_t rhs_length) const {
  if (!collator_)
    CreateCollator();

  return static_cast<Result>(
      ucol_strcoll(collator_, lhs, lhs_length, rhs, rhs_length));
}

void Collator::CreateCollator() const {
  DCHECK(!collator_);
  UErrorCode status = U_ZERO_ERROR;

  {
    Locker<Mutex> lock(CachedCollatorMutex());
    if (g_cached_collator) {
      UColAttributeValue cached_collator_lower_first =
          ucol_getAttribute(g_cached_collator, UCOL_CASE_FIRST, &status);
      DCHECK(U_SUCCESS(status));

      if (0 == strcmp(g_cached_equivalent_locale, equivalent_locale_) &&
          ((UCOL_LOWER_FIRST == cached_collator_lower_first && lower_first_) ||
           (UCOL_UPPER_FIRST == cached_collator_lower_first &&
            !lower_first_))) {
        collator_ = g_cached_collator;
        g_cached_collator = nullptr;
        g_cached_equivalent_locale[0] = 0;
        return;
      }
    }
  }

  collator_ = ucol_open(locale_, &status);
  if (U_FAILURE(status)) {
    status = U_ZERO_ERROR;
    collator_ =
        ucol_open("", &status);  // Fallback to Unicode Collation Algorithm.
  }
  DCHECK(U_SUCCESS(status));

  ucol_setAttribute(collator_, UCOL_CASE_FIRST,
                    lower_first_ ? UCOL_LOWER_FIRST : UCOL_UPPER_FIRST,
                    &status);
  DCHECK(U_SUCCESS(status));

  ucol_setAttribute(collator_, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
  DCHECK(U_SUCCESS(status));
}

void Collator::ReleaseCollator() {
  {
    Locker<Mutex> lock(CachedCollatorMutex());
    if (g_cached_collator)
      ucol_close(g_cached_collator);
    g_cached_collator = collator_;
    strncpy(g_cached_equivalent_locale, equivalent_locale_,
            kUlocFullnameCapacity);
    collator_ = nullptr;
  }
  collator_ = nullptr;
}

void Collator::SetEquivalentLocale(const char* locale,
                                   char* equivalent_locale) {
  UErrorCode status = U_ZERO_ERROR;
  UBool is_available;
  ucol_getFunctionalEquivalent(equivalent_locale, kUlocFullnameCapacity,
                               "collation", locale, &is_available, &status);
  if (U_FAILURE(status))
    strcpy(equivalent_locale, "root");
}

}  // namespace WTF
