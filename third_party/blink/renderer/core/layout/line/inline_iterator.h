/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010 Apple Inc.
 *               All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_INLINE_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_INLINE_ITERATOR_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_inline.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_text.h"
#include "third_party/blink/renderer/core/layout/bidi_run.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

struct BidiIsolatedRun {
  BidiIsolatedRun(LineLayoutItem object,
                  unsigned position,
                  LineLayoutItem& root,
                  BidiRun& run_to_replace,
                  unsigned char level)
      : object(object),
        root(root),
        run_to_replace(run_to_replace),
        position(position),
        level(level) {}

  LineLayoutItem object;
  LineLayoutItem root;
  BidiRun& run_to_replace;
  unsigned position;
  unsigned char level;
};

// This class is used to LayoutInline subtrees, stepping by character within the
// text children. InlineIterator will use bidiNext to find the next LayoutText
// optionally notifying a BidiResolver every time it steps into/out of a
// LayoutInline.
class InlineIterator {
  DISALLOW_NEW();

 public:
  enum IncrementRule {
    kFastIncrementInIsolatedLayout,
    kFastIncrementInTextNode
  };

  InlineIterator()
      : root_(nullptr),
        line_layout_item_(nullptr),
        next_breakable_position_(-1),
        pos_(0) {}

  InlineIterator(LineLayoutItem root, LineLayoutItem o, unsigned p)
      : root_(root),
        line_layout_item_(o),
        next_breakable_position_(-1),
        pos_(p) {}

  void Clear() { MoveTo(nullptr, 0); }

  void MoveToStartOf(LineLayoutItem object) { MoveTo(object, 0); }

  void MoveTo(LineLayoutItem object, unsigned offset, int next_break = -1) {
    line_layout_item_ = object;
    pos_ = offset;
    next_breakable_position_ = next_break;
  }

  LineLayoutItem GetLineLayoutItem() const { return line_layout_item_; }
  void SetLineLayoutItem(LineLayoutItem line_layout_item) {
    line_layout_item_ = line_layout_item;
  }

  int NextBreakablePosition() const { return next_breakable_position_; }
  void SetNextBreakablePosition(int position) {
    next_breakable_position_ = position;
  }

  unsigned Offset() const { return pos_; }
  void SetOffset(unsigned position) { pos_ = position; }
  LineLayoutItem Root() const { return root_; }

  void FastIncrementInTextNode();
  void Increment(InlineBidiResolver* = nullptr,
                 IncrementRule = kFastIncrementInTextNode);
  bool AtEnd() const;

  inline bool AtTextParagraphSeparator() const {
    return line_layout_item_ && line_layout_item_.PreservesNewline() &&
           line_layout_item_.IsText() &&
           LineLayoutText(line_layout_item_).TextLength() &&
           !LineLayoutText(line_layout_item_).IsWordBreak() &&
           LineLayoutText(line_layout_item_).CharacterAt(pos_) == '\n';
  }

  inline bool AtParagraphSeparator() const {
    return (line_layout_item_ && line_layout_item_.IsBR()) ||
           AtTextParagraphSeparator();
  }

  UChar CharacterAt(unsigned) const;
  UChar Current() const;
  UChar PreviousInSameNode() const;
  UChar32 CodepointAt(unsigned) const;
  UChar32 CurrentCodepoint() const;
  ALWAYS_INLINE WTF::unicode::CharDirection Direction() const;

 private:
  LineLayoutItem root_;
  LineLayoutItem line_layout_item_;

  int next_breakable_position_;
  unsigned pos_;
};

inline bool operator==(const InlineIterator& it1, const InlineIterator& it2) {
  return it1.Offset() == it2.Offset() &&
         it1.GetLineLayoutItem() == it2.GetLineLayoutItem();
}

inline bool operator!=(const InlineIterator& it1, const InlineIterator& it2) {
  return it1.Offset() != it2.Offset() ||
         it1.GetLineLayoutItem() != it2.GetLineLayoutItem();
}

static inline WTF::unicode::CharDirection EmbedCharFromDirection(
    TextDirection dir,
    UnicodeBidi unicode_bidi) {
  if (unicode_bidi == UnicodeBidi::kEmbed) {
    return dir == TextDirection::kRtl ? WTF::unicode::kRightToLeftEmbedding
                                      : WTF::unicode::kLeftToRightEmbedding;
  }
  return dir == TextDirection::kRtl ? WTF::unicode::kRightToLeftOverride
                                    : WTF::unicode::kLeftToRightOverride;
}

static inline bool TreatAsIsolated(const ComputedStyle& style) {
  return IsIsolated(style.GetUnicodeBidi()) &&
         style.RtlOrdering() == EOrder::kLogical;
}

template <class Observer>
static inline void NotifyObserverEnteredObject(Observer* observer,
                                               LineLayoutItem object) {
  if (!observer || !object || !object.IsLayoutInline())
    return;

  const ComputedStyle& style = object.StyleRef();
  UnicodeBidi unicode_bidi = style.GetUnicodeBidi();
  if (unicode_bidi == UnicodeBidi::kNormal) {
    // https://drafts.csswg.org/css-writing-modes/#unicode-bidi
    // "The element does not open an additional level of embedding with respect
    // to the bidirectional algorithm."
    // Thus we ignore any possible dir= attribute on the span.
    return;
  }
  if (TreatAsIsolated(style)) {
    // Make sure that explicit embeddings are committed before we enter the
    // isolated content.
    observer->CommitExplicitEmbedding(observer->Runs());
    observer->EnterIsolate();
    // Embedding/Override characters implied by dir= will be handled when
    // we process the isolated span, not when laying out the "parent" run.
    return;
  }

  if (!observer->InIsolate())
    observer->Embed(EmbedCharFromDirection(style.Direction(), unicode_bidi),
                    kFromStyleOrDOM);
}

template <class Observer>
static inline void NotifyObserverWillExitObject(Observer* observer,
                                                LineLayoutItem object) {
  if (!observer || !object || !object.IsLayoutInline())
    return;

  UnicodeBidi unicode_bidi = object.StyleRef().GetUnicodeBidi();
  if (unicode_bidi == UnicodeBidi::kNormal)
    return;  // Nothing to do for unicode-bidi: normal
  if (TreatAsIsolated(object.StyleRef())) {
    observer->ExitIsolate();
    return;
  }

  // Otherwise we pop any embed/override character we added when we opened this
  // tag.
  if (!observer->InIsolate())
    observer->Embed(WTF::unicode::kPopDirectionalFormat, kFromStyleOrDOM);
}

static inline bool IsIteratorTarget(LineLayoutItem object) {
  // The iterator will of course return 0, but its not an expected argument to
  // this function.
  DCHECK(object);
  return object.IsText() || object.IsFloating() ||
         object.IsOutOfFlowPositioned() || object.IsAtomicInlineLevel();
}

// This enum is only used for bidiNextShared()
enum EmptyInlineBehavior {
  kSkipEmptyInlines,
  kIncludeEmptyInlines,
};

static bool IsEmptyInline(LineLayoutItem object) {
  if (!object.IsLayoutInline())
    return false;

  for (LineLayoutItem curr = LineLayoutInline(object).FirstChild(); curr;
       curr = curr.NextSibling()) {
    if (curr.IsFloatingOrOutOfFlowPositioned())
      continue;
    if (curr.IsText() && LineLayoutText(curr).IsAllCollapsibleWhitespace())
      continue;

    if (!IsEmptyInline(curr))
      return false;
  }
  return true;
}

// FIXME: This function is misleadingly named. It has little to do with bidi.
// This function will iterate over inlines within a block, optionally notifying
// a bidi resolver as it enters/exits inlines (so it can push/pop embedding
// levels).
template <class Observer>
static inline LineLayoutItem BidiNextShared(
    LineLayoutItem root,
    LineLayoutItem current,
    Observer* observer = 0,
    EmptyInlineBehavior empty_inline_behavior = kSkipEmptyInlines,
    bool* end_of_inline_ptr = nullptr) {
  LineLayoutItem next = nullptr;
  // oldEndOfInline denotes if when we last stopped iterating if we were at the
  // end of an inline.
  bool old_end_of_inline = end_of_inline_ptr ? *end_of_inline_ptr : false;
  bool end_of_inline = false;

  while (current) {
    next = nullptr;
    if (!old_end_of_inline && !IsIteratorTarget(current)) {
      next = current.SlowFirstChild();
      NotifyObserverEnteredObject(observer, next);
    }

    // We hit this when either current has no children, or when current is not a
    // layoutObject we care about.
    if (!next) {
      // If it is a layoutObject we care about, and we're doing our inline-walk,
      // return it.
      if (empty_inline_behavior == kIncludeEmptyInlines && !old_end_of_inline &&
          current.IsLayoutInline()) {
        next = current;
        end_of_inline = true;
        break;
      }

      while (current && current != root) {
        NotifyObserverWillExitObject(observer, current);

        next = current.NextSibling();
        if (next) {
          NotifyObserverEnteredObject(observer, next);
          break;
        }

        current = current.Parent();
        if (empty_inline_behavior == kIncludeEmptyInlines && current &&
            current != root && current.IsLayoutInline()) {
          next = current;
          end_of_inline = true;
          break;
        }
      }
    }

    if (!next)
      break;

    if (IsIteratorTarget(next) ||
        ((empty_inline_behavior == kIncludeEmptyInlines ||
          IsEmptyInline(next))  // Always return EMPTY inlines.
         && next.IsLayoutInline()))
      break;
    current = next;
  }

  if (end_of_inline_ptr)
    *end_of_inline_ptr = end_of_inline;

  return next;
}

template <class Observer>
static inline LineLayoutItem BidiNextSkippingEmptyInlines(
    LineLayoutItem root,
    LineLayoutItem current,
    Observer* observer) {
  // TODO(rhogan): Rename this caller. It's used for a detailed walk of every
  // object in an inline flow, for example during line layout.
  // We always return empty inlines in bidiNextShared, which gives lie to the
  // bidiNext[Skipping|Including]EmptyInlines naming scheme we use to call it.
  // bidiNextSkippingEmptyInlines is the less fussy of the two callers, it will
  // always try to advance and will return what it finds if it's a line layout
  // object in isIteratorTarget or if it's an empty LayoutInline. If the
  // LayoutInline has content, it will advance past the start of the LayoutLine
  // and try to return one of its children. The SkipEmptyInlines callers never
  // care about endOfInlinePtr.
  return BidiNextShared(root, current, observer, kSkipEmptyInlines);
}

// This makes callers cleaner as they don't have to specify a type for the
// observer when not providing one.
static inline LineLayoutItem BidiNextSkippingEmptyInlines(
    LineLayoutItem root,
    LineLayoutItem current) {
  InlineBidiResolver* observer = nullptr;
  return BidiNextSkippingEmptyInlines(root, current, observer);
}

static inline LineLayoutItem BidiNextIncludingEmptyInlines(
    LineLayoutItem root,
    LineLayoutItem current,
    bool* end_of_inline_ptr = nullptr) {
  // TODO(rhogan): Rename this caller. It's used for quick and dirty walks of
  // inline children by InlineWalker, which isn't interested in the contents of
  // inlines. Use cases include dirtying objects or simplified layout that
  // leaves lineboxes intact. bidiNextIncludingEmptyInlines will return if the
  // iterator is at the start of a LayoutInline (even if it hasn't advanced yet)
  // unless it previously stopped at the start of the same LayoutInline the last
  // time it tried to iterate.
  // If it finds itself inside a LayoutInline that doesn't have anything in
  // isIteratorTarget it will return the enclosing LayoutInline.
  InlineBidiResolver* observer =
      nullptr;  // Callers who include empty inlines, never use an observer.
  return BidiNextShared(root, current, observer, kIncludeEmptyInlines,
                        end_of_inline_ptr);
}

static inline LineLayoutItem BidiFirstSkippingEmptyInlines(
    LineLayoutBlockFlow root,
    BidiRunList<BidiRun>& runs,
    InlineBidiResolver* resolver = nullptr) {
  LineLayoutItem o = root.FirstChild();
  if (!o)
    return nullptr;

  if (o.IsLayoutInline()) {
    NotifyObserverEnteredObject(resolver, o);
    if (!IsEmptyInline(o)) {
      o = BidiNextSkippingEmptyInlines(root, o, resolver);
    } else {
      // Never skip empty inlines.
      if (resolver)
        resolver->CommitExplicitEmbedding(runs);
      return o;
    }
  }

  // FIXME: Unify this with the bidiNext call above.
  if (o && !IsIteratorTarget(o))
    o = BidiNextSkippingEmptyInlines(root, o, resolver);

  if (resolver)
    resolver->CommitExplicitEmbedding(runs);
  return o;
}

// FIXME: This method needs to be renamed when bidiNext finds a good name.
static inline LineLayoutItem BidiFirstIncludingEmptyInlines(
    LineLayoutBlockFlow root) {
  LineLayoutItem o = root.FirstChild();
  // If either there are no children to walk, or the first one is correct
  // then just return it.
  if (!o || o.IsLayoutInline() || IsIteratorTarget(o))
    return o;

  return BidiNextIncludingEmptyInlines(root, o);
}

inline void InlineIterator::FastIncrementInTextNode() {
  DCHECK(line_layout_item_);
  DCHECK(line_layout_item_.IsText());
  DCHECK_LE(pos_, LineLayoutText(line_layout_item_).TextLength());
  if (pos_ < INT_MAX)
    pos_++;
}

// FIXME: This is used by LayoutBlockFlow for simplified layout, and has nothing
// to do with bidi it shouldn't use functions called bidiFirst and bidiNext.
class InlineWalker {
  STACK_ALLOCATED();

 public:
  InlineWalker(LineLayoutBlockFlow root)
      : root_(root), current_(nullptr), at_end_of_inline_(false) {
    // FIXME: This class should be taught how to do the SkipEmptyInlines
    // codepath as well.
    current_ = BidiFirstIncludingEmptyInlines(root_);
  }

  LineLayoutBlockFlow Root() { return root_; }
  LineLayoutItem Current() { return current_; }

  bool AtEndOfInline() { return at_end_of_inline_; }
  bool AtEnd() const { return !current_; }

  LineLayoutItem Advance() {
    // FIXME: Support SkipEmptyInlines and observer parameters.
    current_ =
        BidiNextIncludingEmptyInlines(root_, current_, &at_end_of_inline_);
    return current_;
  }

 private:
  LineLayoutBlockFlow root_;
  LineLayoutItem current_;
  bool at_end_of_inline_;
};

static inline bool EndOfLineHasIsolatedObjectAncestor(
    const InlineIterator& isolated_iterator,
    const InlineIterator& ancestor_itertor) {
  if (!isolated_iterator.GetLineLayoutItem() ||
      !TreatAsIsolated(isolated_iterator.GetLineLayoutItem().StyleRef()))
    return false;

  LineLayoutItem inner_isolated_object = isolated_iterator.GetLineLayoutItem();
  while (inner_isolated_object &&
         inner_isolated_object != isolated_iterator.Root()) {
    if (inner_isolated_object == ancestor_itertor.GetLineLayoutItem())
      return true;
    inner_isolated_object = inner_isolated_object.Parent();
  }
  return false;
}

inline void InlineIterator::Increment(InlineBidiResolver* resolver,
                                      IncrementRule rule) {
  if (!line_layout_item_)
    return;

  if (rule == kFastIncrementInIsolatedLayout && resolver &&
      resolver->InIsolate() &&
      !EndOfLineHasIsolatedObjectAncestor(resolver->EndOfLine(),
                                          resolver->GetPosition())) {
    MoveTo(BidiNextSkippingEmptyInlines(root_, line_layout_item_, resolver), 0);
    return;
  }

  if (line_layout_item_.IsText()) {
    FastIncrementInTextNode();
    if (pos_ < LineLayoutText(line_layout_item_).TextLength())
      return;
  }
  // bidiNext can return 0, so use moveTo instead of moveToStartOf
  MoveTo(BidiNextSkippingEmptyInlines(root_, line_layout_item_, resolver), 0);
}

inline bool InlineIterator::AtEnd() const {
  return !line_layout_item_;
}

inline UChar InlineIterator::CharacterAt(unsigned index) const {
  if (!line_layout_item_ || !line_layout_item_.IsText())
    return 0;

  return LineLayoutText(line_layout_item_).CharacterAt(index);
}

inline UChar InlineIterator::Current() const {
  return CharacterAt(pos_);
}

inline UChar InlineIterator::PreviousInSameNode() const {
  if (!pos_)
    return 0;

  return CharacterAt(pos_ - 1);
}

inline UChar32 InlineIterator::CodepointAt(unsigned index) const {
  if (!line_layout_item_ || !line_layout_item_.IsText())
    return 0;
  return LineLayoutText(line_layout_item_).CodepointAt(index);
}

inline UChar32 InlineIterator::CurrentCodepoint() const {
  return CodepointAt(pos_);
}

ALWAYS_INLINE WTF::unicode::CharDirection InlineIterator::Direction() const {
  if (UChar32 c = CurrentCodepoint())
    return WTF::unicode::Direction(c);

  if (line_layout_item_ && line_layout_item_.IsListMarker()) {
    return line_layout_item_.StyleRef().IsLeftToRightDirection()
               ? WTF::unicode::kLeftToRight
               : WTF::unicode::kRightToLeft;
  }

  return WTF::unicode::kOtherNeutral;
}

template <>
inline void InlineBidiResolver::Increment() {
  current_.Increment(this, InlineIterator::kFastIncrementInIsolatedLayout);
}

template <>
inline bool InlineBidiResolver::IsEndOfLine(const InlineIterator& end) {
  bool in_end_of_line =
      current_ == end || current_.AtEnd() ||
      (InIsolate() && current_.GetLineLayoutItem() == end.GetLineLayoutItem());
  if (InIsolate() && in_end_of_line) {
    current_.MoveTo(current_.GetLineLayoutItem(), end.Offset(),
                    current_.NextBreakablePosition());
    last_ = current_;
    UpdateStatusLastFromCurrentDirection(WTF::unicode::kOtherNeutral);
  }
  return in_end_of_line;
}

static inline bool IsCollapsibleSpace(UChar character,
                                      LineLayoutText layout_text) {
  if (character == ' ' || character == '\t' ||
      character == kSoftHyphenCharacter)
    return true;
  if (character == '\n')
    return !layout_text.StyleRef().PreserveNewline();
  return false;
}

template <typename CharacterType>
static inline int FindFirstTrailingSpace(LineLayoutText last_text,
                                         const CharacterType* characters,
                                         int start,
                                         int stop) {
  int first_space = stop;
  while (first_space > start) {
    UChar current = characters[first_space - 1];
    if (!IsCollapsibleSpace(current, last_text))
      break;
    first_space--;
  }

  return first_space;
}

template <>
inline int InlineBidiResolver::FindFirstTrailingSpaceAtRun(BidiRun* run) {
  DCHECK(run);
  LineLayoutItem last_object = LineLayoutItem(run->line_layout_item_);
  if (!last_object.IsText())
    return run->stop_;

  LineLayoutText last_text(last_object);
  int first_space;
  if (last_text.Is8Bit())
    first_space = FindFirstTrailingSpace(last_text, last_text.Characters8(),
                                         run->Start(), run->Stop());
  else
    first_space = FindFirstTrailingSpace(last_text, last_text.Characters16(),
                                         run->Start(), run->Stop());
  return first_space;
}

template <>
inline BidiRun* InlineBidiResolver::AddTrailingRun(
    BidiRunList<BidiRun>& runs,
    int start,
    int stop,
    BidiRun* run,
    BidiContext* context,
    TextDirection direction) const {
  DCHECK(context);
  BidiRun* new_trailing_run = new BidiRun(
      context->Override(), context->Level(), start, stop,
      run->line_layout_item_, WTF::unicode::kOtherNeutral, context->Dir());
  if (direction == TextDirection::kLtr)
    runs.AddRun(new_trailing_run);
  else
    runs.PrependRun(new_trailing_run);

  return new_trailing_run;
}

template <>
inline bool InlineBidiResolver::NeedsTrailingSpace(BidiRunList<BidiRun>& runs) {
  if (needs_trailing_space_)
    return true;
  const ComputedStyle& style =
      runs.LogicallyLastRun()->line_layout_item_.StyleRef();
  if (style.BreakOnlyAfterWhiteSpace() && style.AutoWrap())
    return true;
  return false;
}

static inline bool IsIsolatedInline(LineLayoutItem object) {
  DCHECK(object);
  return object.IsLayoutInline() && TreatAsIsolated(object.StyleRef());
}

static inline LineLayoutItem HighestContainingIsolateWithinRoot(
    LineLayoutItem object,
    LineLayoutItem root) {
  DCHECK(object);
  LineLayoutItem containing_isolate_obj(nullptr);
  while (object && object != root) {
    if (IsIsolatedInline(object))
      containing_isolate_obj = LineLayoutItem(object);

    object = object.Parent();
    DCHECK(object);
  }
  return containing_isolate_obj;
}

static inline unsigned NumberOfIsolateAncestors(const InlineIterator& iter) {
  LineLayoutItem object = iter.GetLineLayoutItem();
  if (!object)
    return 0;
  unsigned count = 0;
  while (object && object != iter.Root()) {
    if (IsIsolatedInline(object))
      count++;
    object = object.Parent();
  }
  return count;
}

// FIXME: This belongs on InlineBidiResolver, except it's a template
// specialization of BidiResolver which knows nothing about LayoutObjects.
static inline BidiRun* AddPlaceholderRunForIsolatedInline(
    InlineBidiResolver& resolver,
    LineLayoutItem obj,
    unsigned pos,
    LineLayoutItem root) {
  DCHECK(obj);
  BidiRun* isolated_run =
      new BidiRun(resolver.Context()->Override(), resolver.Context()->Level(),
                  pos, pos, obj, resolver.Dir(), resolver.Context()->Dir());
  resolver.Runs().AddRun(isolated_run);
  // FIXME: isolatedRuns() could be a hash of object->run and then we could
  // cheaply ASSERT here that we didn't create multiple objects for the same
  // inline.
  resolver.IsolatedRuns().push_back(BidiIsolatedRun(
      obj, pos, root, *isolated_run, resolver.Context()->Level()));
  return isolated_run;
}

static inline BidiRun* CreateRun(int start,
                                 int end,
                                 LineLayoutItem obj,
                                 InlineBidiResolver& resolver) {
  return new BidiRun(resolver.Context()->Override(),
                     resolver.Context()->Level(), start, end, obj,
                     resolver.Dir(), resolver.Context()->Dir());
}

enum AppendRunBehavior { kAppendingFakeRun, kAppendingRunsForObject };

class IsolateTracker {
  STACK_ALLOCATED();

 public:
  explicit IsolateTracker(BidiRunList<BidiRun>& runs,
                          unsigned nested_isolate_count)
      : nested_isolate_count_(nested_isolate_count),
        have_added_fake_run_for_root_isolate_(false),
        runs_(runs) {}

  void SetMidpointStateForRootIsolate(const LineMidpointState& midpoint_state) {
    midpoint_state_for_root_isolate_ = midpoint_state;
  }

  void EnterIsolate() { nested_isolate_count_++; }
  void ExitIsolate() {
    DCHECK_GE(nested_isolate_count_, 1u);
    nested_isolate_count_--;
    if (!InIsolate())
      have_added_fake_run_for_root_isolate_ = false;
  }
  bool InIsolate() const { return nested_isolate_count_; }

  // We don't care if we encounter bidi directional overrides.
  void Embed(WTF::unicode::CharDirection, BidiEmbeddingSource) {}
  void CommitExplicitEmbedding(BidiRunList<BidiRun>&) {}
  BidiRunList<BidiRun>& Runs() { return runs_; }

  void AddFakeRunIfNecessary(LineLayoutItem obj,
                             unsigned pos,
                             unsigned end,
                             LineLayoutItem root,
                             InlineBidiResolver& resolver) {
    // We only need to add a fake run for a given isolated span once during each
    // call to createBidiRunsForLine. We'll be called for every span inside the
    // isolated span so we just ignore subsequent calls.
    // We also avoid creating a fake run until we hit a child that warrants one,
    // e.g. we skip floats.
    if (LayoutBlockFlow::ShouldSkipCreatingRunsForObject(obj))
      return;
    if (!have_added_fake_run_for_root_isolate_) {
      BidiRun* run =
          AddPlaceholderRunForIsolatedInline(resolver, obj, pos, root);
      resolver.SetMidpointStateForIsolatedRun(*run,
                                              midpoint_state_for_root_isolate_);
      have_added_fake_run_for_root_isolate_ = true;
    }
    // obj and pos together denote a single position in the inline, from which
    // the parsing of the isolate will start.
    // We don't need to mark the end of the run because this is implicit: it is
    // either endOfLine or the end of the isolate, when we call
    // createBidiRunsForLine it will stop at whichever comes first.
  }

 private:
  unsigned nested_isolate_count_;
  bool have_added_fake_run_for_root_isolate_;
  LineMidpointState midpoint_state_for_root_isolate_;
  BidiRunList<BidiRun>& runs_;
};

static void inline AppendRunObjectIfNecessary(LineLayoutItem obj,
                                              unsigned start,
                                              unsigned end,
                                              LineLayoutItem root,
                                              InlineBidiResolver& resolver,
                                              AppendRunBehavior behavior,
                                              IsolateTracker& tracker) {
  // Trailing space code creates empty BidiRun objects, start == end, so
  // that case needs to be handled specifically.
  bool add_empty_run = (end == start);

  // Append BidiRun objects, at most 64K chars at a time, until all
  // text between |start| and |end| is represented.
  while (end > start || add_empty_run) {
    add_empty_run = false;
    const int kLimit =
        USHRT_MAX;  // InlineTextBox stores text length as uint16_t.
    unsigned limited_end = end;
    if (end - start > kLimit)
      limited_end = start + kLimit;
    if (behavior == kAppendingFakeRun)
      tracker.AddFakeRunIfNecessary(obj, start, limited_end, root, resolver);
    else
      resolver.Runs().AddRun(CreateRun(start, limited_end, obj, resolver));
    start = limited_end;
  }
}

static void AdjustMidpointsAndAppendRunsForObjectIfNeeded(
    LineLayoutItem obj,
    unsigned start,
    unsigned end,
    LineLayoutItem root,
    InlineBidiResolver& resolver,
    AppendRunBehavior behavior,
    IsolateTracker& tracker) {
  if (start > end || LayoutBlockFlow::ShouldSkipCreatingRunsForObject(obj))
    return;

  for (;;) {
    LineMidpointState& line_midpoint_state = resolver.GetMidpointState();
    bool have_next_midpoint = (line_midpoint_state.CurrentMidpoint() <
                               line_midpoint_state.NumMidpoints());
    InlineIterator next_midpoint;
    if (have_next_midpoint)
      next_midpoint = line_midpoint_state
                          .Midpoints()[line_midpoint_state.CurrentMidpoint()];
    if (line_midpoint_state.BetweenMidpoints()) {
      if (!(have_next_midpoint && next_midpoint.GetLineLayoutItem() == obj))
        return;
      // This is a new start point. Stop ignoring objects and
      // adjust our start.
      line_midpoint_state.SetBetweenMidpoints(false);
      start = next_midpoint.Offset();
      line_midpoint_state.IncrementCurrentMidpoint();
      if (start < end)
        continue;
    } else {
      if (!have_next_midpoint || (obj != next_midpoint.GetLineLayoutItem())) {
        AppendRunObjectIfNecessary(obj, start, end, root, resolver, behavior,
                                   tracker);
        return;
      }

      // An end midpoint has been encountered within our object. We
      // need to go ahead and append a run with our endpoint.
      if (next_midpoint.Offset() + 1 <= end) {
        line_midpoint_state.SetBetweenMidpoints(true);
        line_midpoint_state.IncrementCurrentMidpoint();
        // UINT_MAX means stop at the object and don't include any of it.
        if (next_midpoint.Offset() != UINT_MAX) {
          if (next_midpoint.Offset() + 1 > start)
            AppendRunObjectIfNecessary(obj, start, next_midpoint.Offset() + 1,
                                       root, resolver, behavior, tracker);
          start = next_midpoint.Offset() + 1;
          continue;
        }
      } else {
        AppendRunObjectIfNecessary(obj, start, end, root, resolver, behavior,
                                   tracker);
      }
    }
    return;
  }
}

static inline void AddFakeRunIfNecessary(LineLayoutItem obj,
                                         unsigned start,
                                         unsigned end,
                                         LineLayoutItem root,
                                         InlineBidiResolver& resolver,
                                         IsolateTracker& tracker) {
  tracker.SetMidpointStateForRootIsolate(resolver.GetMidpointState());
  AdjustMidpointsAndAppendRunsForObjectIfNeeded(
      obj, start, obj.length(), root, resolver, kAppendingFakeRun, tracker);
}

template <>
inline void InlineBidiResolver::AppendRun(BidiRunList<BidiRun>& runs) {
  if (!empty_run_ && !eor_.AtEnd() && !reached_end_of_line_) {
    // Keep track of when we enter/leave "unicode-bidi: isolate" inlines.
    // Initialize our state depending on if we're starting in the middle of such
    // an inline.
    // FIXME: Could this initialize from InIsolate() instead of walking up
    // the layout tree?
    IsolateTracker isolate_tracker(runs, NumberOfIsolateAncestors(sor_));
    int start = sor_.Offset();
    LineLayoutItem obj = sor_.GetLineLayoutItem();
    while (obj && obj != eor_.GetLineLayoutItem() &&
           obj != end_of_run_at_end_of_line_.GetLineLayoutItem()) {
      if (isolate_tracker.InIsolate())
        AddFakeRunIfNecessary(obj, start, obj.length(), sor_.Root(), *this,
                              isolate_tracker);
      else
        AdjustMidpointsAndAppendRunsForObjectIfNeeded(
            obj, start, obj.length(), sor_.Root(), *this,
            kAppendingRunsForObject, isolate_tracker);
      // FIXME: start/obj should be an InlineIterator instead of two separate
      // variables.
      start = 0;
      obj = BidiNextSkippingEmptyInlines(sor_.Root(), obj, &isolate_tracker);
    }
    bool is_end_of_line =
        obj == end_of_line_.GetLineLayoutItem() && !end_of_line_.Offset();
    if (obj && !is_end_of_line) {
      unsigned pos = obj == eor_.GetLineLayoutItem() ? eor_.Offset() : INT_MAX;
      if (obj == end_of_run_at_end_of_line_.GetLineLayoutItem() &&
          end_of_run_at_end_of_line_.Offset() <= pos) {
        reached_end_of_line_ = true;
        pos = end_of_run_at_end_of_line_.Offset();
      }
      // It's OK to add runs for zero-length LayoutObjects, just don't make the
      // run larger than it should be
      int end = obj.length() ? pos + 1 : 0;
      if (isolate_tracker.InIsolate())
        AddFakeRunIfNecessary(obj, start, end, sor_.Root(), *this,
                              isolate_tracker);
      else
        AdjustMidpointsAndAppendRunsForObjectIfNeeded(
            obj, start, end, sor_.Root(), *this, kAppendingRunsForObject,
            isolate_tracker);
    }

    if (is_end_of_line)
      reached_end_of_line_ = true;
    // If isolate_tracker is InIsolate, the next |start of run| can not be the
    // current isolated layoutObject.
    if (isolate_tracker.InIsolate())
      eor_.MoveTo(
          BidiNextSkippingEmptyInlines(eor_.Root(), eor_.GetLineLayoutItem()),
          0);
    else
      eor_.Increment();
    sor_ = eor_;
  }

  direction_ = WTF::unicode::kOtherNeutral;
  status_.eor = WTF::unicode::kOtherNeutral;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_INLINE_ITERATOR_H_
