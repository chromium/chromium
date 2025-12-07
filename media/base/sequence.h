// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SEQUENCE_H_
#define MEDIA_BASE_SEQUENCE_H_

#include <cstddef>
#include <optional>
#include <ranges>
#include <utility>
#include <variant>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"

namespace media::sequence {

template <typename C, typename T>
concept Sequence = std::ranges::forward_range<C> &&
                   std::same_as<std::ranges::range_value_t<C>, T>;

namespace detail {

template <typename Sequenceable>
class ReferenceSequence {
 public:
  using value_type = typename std::ranges::range_value_t<Sequenceable>;

  constexpr explicit ReferenceSequence(const Sequenceable& s) : s_(s) {}
  constexpr ReferenceSequence(const ReferenceSequence&) = default;
  constexpr ReferenceSequence& operator=(const ReferenceSequence&) = default;

  constexpr auto begin() const { return s_->begin(); }
  constexpr auto end() const { return s_->end(); }

 private:
  raw_ref<const Sequenceable> s_;
};

template <typename T>
class SingletSequenceIterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = T;

  SingletSequenceIterator() = default;
  constexpr explicit SingletSequenceIterator(const T* t, bool end = false)
      : t_(t), end_(end) {}

  constexpr const value_type& operator*() const { return *t_; }
  constexpr const value_type* operator->() const { return t_; }

  constexpr SingletSequenceIterator& operator++() {
    t_ = nullptr;
    end_ = true;
    return *this;
  }

  constexpr SingletSequenceIterator operator++(int) {
    SingletSequenceIterator temp = *this;
    ++(*this);
    return temp;
  }

  constexpr bool operator==(const SingletSequenceIterator& other) const {
    return t_ == other.t_;
  }

 private:
  raw_ptr<const T> t_;
  bool end_;
};

template <typename T, bool Owned>
class SingletSequence {
 public:
  using value_type = T;

  constexpr explicit SingletSequence(T&& t)
    requires(Owned)
      : storage_(t) {}
  constexpr explicit SingletSequence(T& t)
    requires(!Owned)
      : storage_(&t) {}

  static constexpr SingletSequence EmptySinglet() { return SingletSequence(); }

  constexpr SingletSequenceIterator<T> begin() const {
    const T* ptr = GetDataPtr();
    return SingletSequenceIterator<T>(ptr, ptr != nullptr);
  }

  constexpr SingletSequenceIterator<T> end() const {
    return SingletSequenceIterator<T>(nullptr, true);
  }

 private:
  std::conditional_t<Owned, const std::optional<T>, raw_ptr<const T>> storage_;

  constexpr const T* GetDataPtr() const {
    if constexpr (Owned) {
      return storage_.has_value() ? &*storage_ : nullptr;
    } else {
      return storage_;
    }
  }

  constexpr explicit SingletSequence() = default;
};

template <typename R, typename... Rs>
struct ConcatSequenceIterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = typename std::ranges::range_value_t<R>;
  using difference_type = std::ptrdiff_t;

  ConcatSequenceIterator() = default;

  constexpr ConcatSequenceIterator(const std::tuple<R, Rs...>* view,
                                   size_t index)
      : sub_(view), index_(index) {
    if (!IsEnd()) {
      InitializeIter();
      AdvanceIfEmpty();
    }
  }

  constexpr const value_type& operator*() const {
    return std::visit([](auto&& i) -> const value_type& { return *i; }, iter_);
  }

  constexpr const value_type* operator->() const {
    return std::visit([](auto&& i) -> const value_type* { return &*i; }, iter_);
  }

  constexpr ConcatSequenceIterator& operator++() {
    std::visit([](auto&& i) { ++i; }, iter_);
    AdvanceIfEmpty();
    return *this;
  }

  constexpr ConcatSequenceIterator operator++(int) {
    ConcatSequenceIterator temp = *this;
    ++(*this);
    return temp;
  }

  constexpr bool operator==(const ConcatSequenceIterator& other) const {
    if (IsEnd() && other.IsEnd()) {
      return true;
    }
    if (index_ != other.index_) {
      return false;
    }
    return iter_ == other.iter_;
  }

 private:
  using VariantIter = std::variant<std::ranges::iterator_t<const R>,
                                   std::ranges::iterator_t<const Rs>...>;

  raw_ptr<const std::tuple<R, Rs...>> sub_ = nullptr;
  size_t index_ = 0;
  VariantIter iter_;

  constexpr bool IsEnd() const { return index_ >= 1 + sizeof...(Rs); }

  constexpr void InitializeIter() {
    VisitAt<0>(index_, [this](auto I) {
      iter_.template emplace<I>(std::get<I>(*sub_).begin());
    });
  }

  constexpr bool CurrentIterIsRangeEnd() const {
    return std::visit(
        [this](auto&& i) {
          return VisitAt<0>(iter_.index(), [&](auto I) {
            auto end_iter = std::get<I>(*sub_).end();
            if constexpr (std::is_same_v<std::decay_t<decltype(i)>,
                                         decltype(end_iter)>) {
              return i == end_iter;
            } else {
              return false;
            }
          });
        },
        iter_);
  }

  constexpr void AdvanceIfEmpty() {
    while (!IsEnd() && CurrentIterIsRangeEnd()) {
      index_++;
      if (!IsEnd()) {
        InitializeIter();
      }
    }
  }

  // Recursive helper to turn Runtime Index -> Compile Time Index
  template <size_t I, typename Func>
  static constexpr auto VisitAt(size_t runtime_index, Func&& f) {
    if (runtime_index == I) {
      return f(std::integral_constant<size_t, I>{});
    }

    if constexpr (I < sizeof...(Rs)) {
      return VisitAt<I + 1>(runtime_index, std::forward<Func>(f));
    } else {
      return f(std::integral_constant<size_t, 0>{});
    }
  }
};

template <typename R, typename... Rs>
struct ConcatSequence {
 public:
  using value_type = typename std::ranges::range_value_t<R>;

  static_assert(
      (std::same_as<value_type, typename std::ranges::range_value_t<Rs>> &&
       ...),
      "All concatenated sequences must have the same value_type");

  ConcatSequence() = default;

  constexpr explicit ConcatSequence(R r, Rs... rs)
      : subsequences_{std::move(r), std::move(rs)...} {}

  constexpr ConcatSequenceIterator<R, Rs...> begin() const {
    return ConcatSequenceIterator<R, Rs...>{&subsequences_, 0};
  }

  constexpr ConcatSequenceIterator<R, Rs...> end() const {
    return ConcatSequenceIterator<R, Rs...>{&subsequences_, 1 + sizeof...(Rs)};
  }

 private:
  std::tuple<R, Rs...> subsequences_;
};

}  // namespace detail

template <typename T>
decltype(auto) Reference(T&& t) {
  // If `t` is an lvalue reference type (a move only type for example), then
  // this returns a non-owning view sequence over `t`.
  // If `t` is an rvalue, this returns a decayed version of the iterable.
  if constexpr (std::is_lvalue_reference_v<T>) {
    return detail::ReferenceSequence<std::remove_reference_t<T>>(t);
  } else {
    return std::forward<T>(t);
  }
}

template <typename T>
auto Singlet(T&& t) {
  return detail::SingletSequence<std::remove_reference_t<T>,
                                 !std::is_lvalue_reference_v<T>>(
      std::forward<T>(t));
}

template <typename T>
auto EmptySinglet() {
  return detail::SingletSequence<
      std::remove_reference_t<T>,
      !std::is_lvalue_reference_v<T>>::EmptySinglet();
}

template <typename... Rs>
auto Concat(Rs&&... rs) {
  return detail::ConcatSequence{Reference(std::forward<Rs>(rs))...};
}

}  // namespace media::sequence

#endif  // MEDIA_BASE_SEQUENCE_H_
