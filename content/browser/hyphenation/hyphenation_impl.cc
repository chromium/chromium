// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/hyphenation/hyphenation_impl.h"

#include <map>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace {

struct Dictionaries {
  static Dictionaries* Get() {
    static base::NoDestructor<Dictionaries> dictionaries;
    return dictionaries.get();
  }

#if !BUILDFLAG(IS_ANDROID)
  void SetDirectory(const base::FilePath& new_dir) {
    DVLOG(1) << __func__ << " " << new_dir;
    DCHECK(hyphenation::HyphenationImpl::GetTaskRunner()
               ->RunsTasksInCurrentSequence());
    DCHECK(!new_dir.empty());
    if (new_dir == dir || !base::PathExists(new_dir))
      return;
    dir = new_dir;
    cache.clear();
  }

  base::FilePath dir;
#endif

  // Keep the files open in the cache for subsequent calls.
  std::unordered_map<std::string, base::File> cache;
};

bool IsValidLocale(const std::string& locale) {
  return base::ranges::all_of(locale, [](const char ch) {
    return base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch) || ch == '-';
  });
}

base::File GetDictionaryFile(const std::string& locale) {
  DCHECK(hyphenation::HyphenationImpl::GetTaskRunner()
             ->RunsTasksInCurrentSequence());
  Dictionaries* dictionaries = Dictionaries::Get();
#if !BUILDFLAG(IS_ANDROID)
  const base::FilePath& dir = dictionaries->dir;
  if (dir.empty())
    return base::File();
#endif

  const auto& inserted =
      dictionaries->cache.insert(std::make_pair(locale, base::File()));
  base::File& file = inserted.first->second;
  // If the |locale| is already in the cache, duplicate the file and return it.
  if (!inserted.second)
    return file.Duplicate();
  DCHECK(!file.IsValid());

#if BUILDFLAG(IS_ANDROID)
  base::FilePath dir("/system/usr/hyphen-data");
#endif
  std::string filename = base::StringPrintf("hyph-%s.hyb", locale.c_str());
  base::FilePath path = dir.AppendASCII(filename);
  base::ElapsedTimer timer;
  file.Initialize(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  UMA_HISTOGRAM_TIMES("Hyphenation.Open.File", timer.Elapsed());
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
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
           base::TaskPriority::USER_BLOCKING}));
  return *runner;
}

#if !BUILDFLAG(IS_ANDROID)
// static
void HyphenationImpl::RegisterGetDictionary() {
  content::ContentBrowserClient* content_browser_client =
      content::GetContentClient()->browser();
  DCHECK(content_browser_client);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static bool registered = false;
  if (registered)
    return;
  registered = true;
  content_browser_client->GetHyphenationDictionary(
      base::BindOnce(SetDirectory));
}

// static
void HyphenationImpl::SetDirectory(const base::FilePath& dir) {
  GetTaskRunner()->PostTask(FROM_HERE,
                            base::BindOnce(
                                [](const base::FilePath& dir) {
                                  Dictionaries::Get()->SetDirectory(dir);
                                },
                                dir));
}
#endif

void HyphenationImpl::OpenDictionary(const std::string& locale,
                                     OpenDictionaryCallback callback) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (IsValidLocale(locale))
    std::move(callback).Run(GetDictionaryFile(locale));
  else
    std::move(callback).Run(base::File());
}

}  // namespace hyphenation
