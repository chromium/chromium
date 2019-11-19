// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/hyphenation/hyphenation_impl.h"

#include <algorithm>
#include <map>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace {

using DictionaryFileMap = std::unordered_map<std::string, base::File>;

bool IsValidLocale(const std::string& locale) {
  return std::all_of(locale.cbegin(), locale.cend(), [](const char ch) {
    return base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch) || ch == '-';
  });
}

base::File GetDictionaryFile(const std::string& locale) {
  // Keep Files open in the cache for subsequent calls.
  static base::NoDestructor<DictionaryFileMap> cache;

  const auto& it = cache->find(locale);
  if (it != cache->end())
    return it->second.Duplicate();
  const auto& inserted = cache->insert(std::make_pair(locale, base::File()));
  base::File& file = inserted.first->second;
  DCHECK(!file.IsValid());

#if defined(OS_ANDROID)
  base::FilePath dir("/system/usr/hyphen-data");
#else
#error "This configuration is not supported."
#endif
  std::string filename = base::StringPrintf("hyph-%s.hyb", locale.c_str());
  base::FilePath path = dir.AppendASCII(filename);
  file.Initialize(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  return file.Duplicate();
}

}  // namespace

namespace hyphenation {

HyphenationImpl::HyphenationImpl() {}

HyphenationImpl::~HyphenationImpl() {}

// static
void HyphenationImpl::Create(
    mojo::PendingReceiver<blink::mojom::Hyphenation> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<HyphenationImpl>(),
                              std::move(receiver));
}

// static
scoped_refptr<base::SequencedTaskRunner> HyphenationImpl::GetTaskRunner() {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>> runner(
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
           base::TaskPriority::USER_BLOCKING}));
  return *runner;
}

void HyphenationImpl::OpenDictionary(const std::string& locale,
                                     OpenDictionaryCallback callback) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (IsValidLocale(locale))
    std::move(callback).Run(GetDictionaryFile(locale));
  else
    std::move(callback).Run(base::File());
}

}  // namespace hyphenation
