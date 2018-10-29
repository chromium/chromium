// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/script_run_iterator.h"

#include <algorithm>
#include "third_party/blink/renderer/platform/text/icu_error.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

typedef ScriptData::PairedBracketType PairedBracketType;

constexpr int ScriptRunIterator::kMaxScriptCount;
constexpr int ScriptData::kMaxScriptCount;

ScriptData::~ScriptData() = default;

void ICUScriptData::GetScripts(UChar32 ch, UScriptCodeList& dst) const {
  ICUError status;
  // Leave room to insert primary script. It's not strictly necessary but
  // it ensures that the result won't ever be greater than kMaxScriptCount,
  // which some client someday might expect.
  dst.resize(kMaxScriptCount - 1);
  // Note, ICU convention is to return the number of available items
  // regardless of the capacity passed to the call. So count can be greater
  // than dst->size(), if a later version of the unicode data has more
  // than kMaxScriptCount items.
  int count = uscript_getScriptExtensions(ch, &dst[0], dst.size(), &status);
  if (status == U_BUFFER_OVERFLOW_ERROR) {
    // Allow this, we'll just use what we have.
    DLOG(ERROR) << "Exceeded maximum script count of " << kMaxScriptCount
                << " for 0x" << std::hex << ch;
    count = dst.size();
    status = U_ZERO_ERROR;
  }
  UScriptCode primary_script = uscript_getScript(ch, &status);

  if (U_FAILURE(status)) {
    DLOG(ERROR) << "Could not get icu script data: " << status << " for 0x"
                << std::hex << ch;
    dst.clear();
    return;
  }

  dst.resize(count);

  if (primary_script == dst.at(0)) {
    // Only one script (might be common or inherited -- these are never in
    // the extensions unless they're the only script), or extensions are in
    // priority order already.
    return;
  }

  if (primary_script != USCRIPT_INHERITED && primary_script != USCRIPT_COMMON &&
      primary_script != USCRIPT_INVALID_CODE) {
    // Not common or primary, with extensions that are not in order. We know
    // the primary, so we insert it at the front and swap the previous front
    // to somewhere else in the list.
    auto* it = std::find(dst.begin() + 1, dst.end(), primary_script);
    if (it == dst.end()) {
      dst.push_back(primary_script);
    }
    std::swap(*dst.begin(), *it);
    return;
  }

  if (primary_script == USCRIPT_COMMON) {
    if (count == 1) {
      // Common with a preferred script. Keep common at head.
      dst.push_front(primary_script);
      return;
    }

    // Ignore common. Find the preferred script of the multiple scripts that
    // remain, and ensure it is at the head. Just keep swapping them in,
    // there aren't likely to be many.
    for (wtf_size_t i = 1; i < dst.size(); ++i) {
      if (dst.at(0) == USCRIPT_LATIN || dst.at(i) < dst.at(0)) {
        std::swap(dst.at(0), dst.at(i));
      }
    }
    return;
  }

  // The primary is inherited, and there are other scripts. Put inherited at
  // the front, the true primary next, and then the others in random order.
  // TODO: Take into account the language of a document if available.
  // Otherwise, use Unicode block as a tie breaker. Comparing
  // ScriptCodes as integers is not meaningful because 'old' scripts are
  // just sorted in alphabetic order.
  dst.push_back(dst.at(0));
  dst.at(0) = primary_script;
  for (wtf_size_t i = 2; i < dst.size(); ++i) {
    if (dst.at(1) == USCRIPT_LATIN || dst.at(i) < dst.at(1)) {
      std::swap(dst.at(1), dst.at(i));
    }
  }
}

UChar32 ICUScriptData::GetPairedBracket(UChar32 ch) const {
  return u_getBidiPairedBracket(ch);
}

PairedBracketType ICUScriptData::GetPairedBracketType(UChar32 ch) const {
  return static_cast<PairedBracketType>(
      u_getIntPropertyValue(ch, UCHAR_BIDI_PAIRED_BRACKET_TYPE));
}

const ICUScriptData* ICUScriptData::Instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const ICUScriptData, icu_script_data_instance,
                                  ());
  return &icu_script_data_instance;
}

ScriptRunIterator::ScriptRunIterator(const UChar* text,
                                     wtf_size_t length,
                                     const ScriptData* data)
    : text_(text),
      length_(length),
      brackets_fixup_depth_(0),
      next_set_(std::make_unique<UScriptCodeList>()),
      ahead_set_(std::make_unique<UScriptCodeList>()),
      // The initial value of m_aheadCharacter is not used.
      ahead_character_(0),
      ahead_pos_(0),
      common_preferred_(USCRIPT_COMMON),
      script_data_(data) {
  DCHECK(text);
  DCHECK(data);

  if (ahead_pos_ < length_) {
    current_set_.clear();
    // Priming the m_currentSet with USCRIPT_COMMON here so that the first
    // resolution between m_currentSet and m_nextSet in mergeSets() leads to
    // chosing the script of the first consumed character.
    current_set_.push_back(USCRIPT_COMMON);
    U16_NEXT(text_, ahead_pos_, length_, ahead_character_);
    script_data_->GetScripts(ahead_character_, *ahead_set_);
  }
}

ScriptRunIterator::ScriptRunIterator(const UChar* text, wtf_size_t length)
    : ScriptRunIterator(text, length, ICUScriptData::Instance()) {}

bool ScriptRunIterator::Consume(unsigned& limit, UScriptCode& script) {
  if (current_set_.IsEmpty()) {
    return false;
  }

  wtf_size_t pos;
  UChar32 ch;
  while (Fetch(&pos, &ch)) {
    PairedBracketType paired_type = script_data_->GetPairedBracketType(ch);
    switch (paired_type) {
      case PairedBracketType::kBracketTypeOpen:
        OpenBracket(ch);
        break;
      case PairedBracketType::kBracketTypeClose:
        CloseBracket(ch);
        break;
      default:
        break;
    }
    if (!MergeSets()) {
      limit = pos;
      script = ResolveCurrentScript();
      FixupStack(script);
      current_set_ = *next_set_;
      return true;
    }
  }

  limit = length_;
  script = ResolveCurrentScript();
  current_set_.clear();
  return true;
}

void ScriptRunIterator::OpenBracket(UChar32 ch) {
  if (brackets_.size() == kMaxBrackets) {
    brackets_.pop_front();
    if (brackets_fixup_depth_ == kMaxBrackets) {
      --brackets_fixup_depth_;
    }
  }
  brackets_.push_back(BracketRec({ch, USCRIPT_COMMON}));
  ++brackets_fixup_depth_;
}

void ScriptRunIterator::CloseBracket(UChar32 ch) {
  if (brackets_.size() > 0) {
    UChar32 target = script_data_->GetPairedBracket(ch);
    for (auto it = brackets_.rbegin(); it != brackets_.rend(); ++it) {
      if (it->ch == target) {
        // Have a match, use open paren's resolved script.
        UScriptCode script = it->script;
        next_set_->clear();
        next_set_->push_back(script);

        // And pop stack to this point.
        int num_popped =
            static_cast<int>(std::distance(brackets_.rbegin(), it));
        // TODO: No resize operation in WTF::Deque?
        for (int i = 0; i < num_popped; ++i)
          brackets_.pop_back();
        brackets_fixup_depth_ = static_cast<wtf_size_t>(
            std::max(0, static_cast<int>(brackets_fixup_depth_) - num_popped));
        return;
      }
    }
  }
  // leave stack alone, no match
}

// Keep items in m_currentSet that are in m_nextSet.
//
// If the sets are disjoint, return false and leave m_currentSet unchanged. Else
// return true and make current set the intersection. Make sure to maintain
// current priority script as priority if it remains, else retain next priority
// script if it remains.
//
// Also maintain a common preferred script.  If current and next are both
// common, and there is no common preferred script and next has a preferred
// script, set the common preferred script to that of next.
bool ScriptRunIterator::MergeSets() {
  if (next_set_->IsEmpty() || current_set_.IsEmpty()) {
    return false;
  }

  auto* current_set_it = current_set_.begin();
  auto* current_end = current_set_.end();
  // Most of the time, this is the only one.
  // Advance the current iterator, we won't need to check it again later.
  UScriptCode priority_script = *current_set_it++;

  // If next is common or inherited, the only thing that might change
  // is the common preferred script.
  if (next_set_->at(0) <= USCRIPT_INHERITED) {
    if (next_set_->size() == 2 && priority_script <= USCRIPT_INHERITED &&
        common_preferred_ == USCRIPT_COMMON) {
      common_preferred_ = next_set_->at(1);
    }
    return true;
  }

  // If current is common or inherited, use the next script set.
  if (priority_script <= USCRIPT_INHERITED) {
    current_set_ = *next_set_;
    return true;
  }

  // Neither is common or inherited. If current is a singleton,
  // just see if it exists in the next set. This is the common case.
  auto* next_it = next_set_->begin();
  auto* next_end = next_set_->end();
  if (current_set_it == current_end) {
    return std::find(next_it, next_end, priority_script) != next_end;
  }

  // Establish the priority script, if we have one.
  // First try current priority script.
  bool have_priority =
      std::find(next_it, next_end, priority_script) != next_end;
  if (!have_priority) {
    // So try next priority script.
    // Skip the first current script, we already know it's not there.
    // Advance the next iterator, later we won't need to check it again.
    priority_script = *next_it++;
    have_priority =
        std::find(current_set_it, current_end, priority_script) != current_end;
  }

  // Note that we can never write more scripts into the current vector than
  // it already contains, so currentWriteIt won't ever exceed the size/capacity.
  auto* current_write_it = current_set_.begin();
  if (have_priority) {
    // keep the priority script.
    *current_write_it++ = priority_script;
  }

  if (next_it != next_end) {
    // Iterate over the remaining current scripts, and keep them if
    // they occur in the remaining next scripts.
    while (current_set_it != current_end) {
      UScriptCode sc = *current_set_it++;
      if (std::find(next_it, next_end, sc) != next_end) {
        *current_write_it++ = sc;
      }
    }
  }

  // Only change current if the run continues.
  int written =
      static_cast<int>(std::distance(current_set_.begin(), current_write_it));
  if (written > 0) {
    current_set_.resize(written);
    return true;
  }
  return false;
}

// When we hit the end of the run, and resolve the script, we now know the
// resolved script of any open bracket that was pushed on the stack since
// the start of the run. Fixup depth records how many of these there
// were. We've maintained this count during pushes, and taken care to
// adjust it if the stack got overfull and open brackets were pushed off
// the bottom. This sets the script of the fixup_depth topmost entries of the
// stack to the resolved script.
void ScriptRunIterator::FixupStack(UScriptCode resolved_script) {
  if (brackets_fixup_depth_ > 0) {
    if (brackets_fixup_depth_ > brackets_.size()) {
      // Should never happen unless someone breaks the code.
      DLOG(ERROR) << "Brackets fixup depth exceeds size of bracket vector.";
      brackets_fixup_depth_ = brackets_.size();
    }
    auto it = brackets_.rbegin();
    for (wtf_size_t i = 0; i < brackets_fixup_depth_; ++i) {
      it->script = resolved_script;
      ++it;
    }
    brackets_fixup_depth_ = 0;
  }
}

bool ScriptRunIterator::Fetch(wtf_size_t* pos, UChar32* ch) {
  if (ahead_pos_ > length_) {
    return false;
  }
  *pos = ahead_pos_ - (ahead_character_ >= 0x10000 ? 2 : 1);
  *ch = ahead_character_;

  std::swap(next_set_, ahead_set_);
  if (ahead_pos_ == length_) {
    // No more data to fetch, but last character still needs to be
    // processed. Advance m_aheadPos so that next time we will know
    // this has been done.
    ahead_pos_++;
    return true;
  }

  U16_NEXT(text_, ahead_pos_, length_, ahead_character_);
  script_data_->GetScripts(ahead_character_, *ahead_set_);
  if (ahead_set_->IsEmpty()) {
    // No scripts for this character. This has already been logged, so
    // we just terminate processing this text.
    return false;
  }
  if ((*ahead_set_)[0] == USCRIPT_INHERITED && ahead_set_->size() > 1) {
    if ((*next_set_)[0] == USCRIPT_COMMON) {
      // Overwrite the next set with the non-inherited portion of the set.
      *next_set_ = *ahead_set_;
      next_set_->EraseAt(0);
      // Discard the remaining values, we'll inherit.
      ahead_set_->resize(1);
    } else {
      // Else, this applies to anything.
      ahead_set_->resize(1);
    }
  }
  return true;
}

UScriptCode ScriptRunIterator::ResolveCurrentScript() const {
  UScriptCode result = current_set_.at(0);
  return result == USCRIPT_COMMON ? common_preferred_ : result;
}

}  // namespace blink
