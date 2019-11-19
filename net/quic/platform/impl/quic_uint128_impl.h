// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_UINT128_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_UINT128_IMPL_H_

#include <stdint.h>

namespace quic {

// An unsigned 128-bit integer type. Thread-compatible.
class QuicUint128Impl {
 public:
  constexpr QuicUint128Impl();  // Sets to 0, but don't trust on this behavior.
  constexpr QuicUint128Impl(uint64_t top, uint64_t bottom);
  QuicUint128Impl(int bottom);
  constexpr QuicUint128Impl(uint32_t bottom);  // Top 96 bits = 0
  constexpr QuicUint128Impl(uint64_t bottom);  // hi_ = 0
  constexpr QuicUint128Impl(const QuicUint128Impl& val);

  void Initialize(uint64_t top, uint64_t bottom);

  QuicUint128Impl& operator=(const QuicUint128Impl& b);

  // Arithmetic operators.
  // TODO: division, etc.
  QuicUint128Impl& operator+=(const QuicUint128Impl& b);
  QuicUint128Impl& operator-=(const QuicUint128Impl& b);
  QuicUint128Impl& operator*=(const QuicUint128Impl& b);
  QuicUint128Impl operator++(int);
  QuicUint128Impl operator--(int);
  QuicUint128Impl& operator<<=(int);
  QuicUint128Impl& operator>>=(int);
  QuicUint128Impl& operator&=(const QuicUint128Impl& b);
  QuicUint128Impl& operator|=(const QuicUint128Impl& b);
  QuicUint128Impl& operator^=(const QuicUint128Impl& b);
  QuicUint128Impl& operator++();
  QuicUint128Impl& operator--();

  friend uint64_t QuicUint128Low64Impl(const QuicUint128Impl& v);
  friend uint64_t QuicUint128High64Impl(const QuicUint128Impl& v);

 private:
  // Little-endian memory order optimizations can benefit from
  // having lo_ first, hi_ last.
  // See util/endian/endian.h and Load128/Store128 for storing a
  // QuicUint128Impl.
  uint64_t lo_;
  uint64_t hi_;

  // Not implemented, just declared for catching automatic type conversions.
  QuicUint128Impl(uint8_t);
  QuicUint128Impl(uint16_t);
  QuicUint128Impl(float v);
  QuicUint128Impl(double v);
};

inline QuicUint128Impl MakeQuicUint128Impl(uint64_t top, uint64_t bottom) {
  return QuicUint128Impl(top, bottom);
}

// Methods to access low and high pieces of 128-bit value.
// Defined externally from QuicUint128Impl to facilitate conversion
// to native 128-bit types when compilers support them.
inline uint64_t QuicUint128Low64Impl(const QuicUint128Impl& v) {
  return v.lo_;
}
inline uint64_t QuicUint128High64Impl(const QuicUint128Impl& v) {
  return v.hi_;
}

// --------------------------------------------------------------------------
//                      Implementation details follow
// --------------------------------------------------------------------------
inline bool operator==(const QuicUint128Impl& lhs, const QuicUint128Impl& rhs) {
  return (QuicUint128Low64Impl(lhs) == QuicUint128Low64Impl(rhs) &&
          QuicUint128High64Impl(lhs) == QuicUint128High64Impl(rhs));
}
inline bool operator!=(const QuicUint128Impl& lhs, const QuicUint128Impl& rhs) {
  return !(lhs == rhs);
}
inline QuicUint128Impl& QuicUint128Impl::operator=(const QuicUint128Impl& b) {
  lo_ = b.lo_;
  hi_ = b.hi_;
  return *this;
}

constexpr QuicUint128Impl::QuicUint128Impl() : lo_(0), hi_(0) {}
constexpr QuicUint128Impl::QuicUint128Impl(uint64_t top, uint64_t bottom)
    : lo_(bottom), hi_(top) {}
constexpr QuicUint128Impl::QuicUint128Impl(const QuicUint128Impl& v)
    : lo_(v.lo_), hi_(v.hi_) {}
constexpr QuicUint128Impl::QuicUint128Impl(uint64_t bottom)
    : lo_(bottom), hi_(0) {}
constexpr QuicUint128Impl::QuicUint128Impl(uint32_t bottom)
    : lo_(bottom), hi_(0) {}
inline QuicUint128Impl::QuicUint128Impl(int bottom) : lo_(bottom), hi_(0) {
  if (bottom < 0) {
    --hi_;
  }
}
inline void QuicUint128Impl::Initialize(uint64_t top, uint64_t bottom) {
  hi_ = top;
  lo_ = bottom;
}

// Comparison operators.

#define CMP128(op)                                                           \
  inline bool operator op(const QuicUint128Impl& lhs,                        \
                          const QuicUint128Impl& rhs) {                      \
    return (QuicUint128High64Impl(lhs) == QuicUint128High64Impl(rhs))        \
               ? (QuicUint128Low64Impl(lhs) op QuicUint128Low64Impl(rhs))    \
               : (QuicUint128High64Impl(lhs) op QuicUint128High64Impl(rhs)); \
  }

CMP128(<)
CMP128(>)
CMP128(>=)
CMP128(<=)

#undef CMP128

// Unary operators

inline QuicUint128Impl operator-(const QuicUint128Impl& val) {
  const uint64_t hi_flip = ~QuicUint128High64Impl(val);
  const uint64_t lo_flip = ~QuicUint128Low64Impl(val);
  const uint64_t lo_add = lo_flip + 1;
  if (lo_add < lo_flip) {
    return QuicUint128Impl(hi_flip + 1, lo_add);
  }
  return QuicUint128Impl(hi_flip, lo_add);
}

inline bool operator!(const QuicUint128Impl& val) {
  return !QuicUint128High64Impl(val) && !QuicUint128Low64Impl(val);
}

// Logical operators.

inline QuicUint128Impl operator~(const QuicUint128Impl& val) {
  return QuicUint128Impl(~QuicUint128High64Impl(val),
                         ~QuicUint128Low64Impl(val));
}

#define LOGIC128(op)                                               \
  inline QuicUint128Impl operator op(const QuicUint128Impl& lhs,   \
                                     const QuicUint128Impl& rhs) { \
    return QuicUint128Impl(                                        \
        QuicUint128High64Impl(lhs) op QuicUint128High64Impl(rhs),  \
        QuicUint128Low64Impl(lhs) op QuicUint128Low64Impl(rhs));   \
  }

LOGIC128(|)
LOGIC128(&)
LOGIC128(^)

#undef LOGIC128

#define LOGICASSIGN128(op)                              \
  inline QuicUint128Impl& QuicUint128Impl::operator op( \
      const QuicUint128Impl& other) {                   \
    hi_ op other.hi_;                                   \
    lo_ op other.lo_;                                   \
    return *this;                                       \
  }

LOGICASSIGN128(|=)
LOGICASSIGN128(&=)
LOGICASSIGN128(^=)

#undef LOGICASSIGN128

// Shift operators.

inline QuicUint128Impl operator<<(const QuicUint128Impl& val, int amount) {
  // uint64_t shifts of >= 64 are undefined, so we will need some
  // special-casing.
  if (amount < 64) {
    if (amount == 0) {
      return val;
    }
    uint64_t new_hi = (QuicUint128High64Impl(val) << amount) |
                      (QuicUint128Low64Impl(val) >> (64 - amount));
    uint64_t new_lo = QuicUint128Low64Impl(val) << amount;
    return QuicUint128Impl(new_hi, new_lo);
  } else if (amount < 128) {
    return QuicUint128Impl(QuicUint128Low64Impl(val) << (amount - 64), 0);
  } else {
    return QuicUint128Impl(0, 0);
  }
}

inline QuicUint128Impl operator>>(const QuicUint128Impl& val, int amount) {
  // uint64_t shifts of >= 64 are undefined, so we will need some
  // special-casing.
  if (amount < 64) {
    if (amount == 0) {
      return val;
    }
    uint64_t new_hi = QuicUint128High64Impl(val) >> amount;
    uint64_t new_lo = (QuicUint128Low64Impl(val) >> amount) |
                      (QuicUint128High64Impl(val) << (64 - amount));
    return QuicUint128Impl(new_hi, new_lo);
  } else if (amount < 128) {
    return QuicUint128Impl(0, QuicUint128High64Impl(val) >> (amount - 64));
  } else {
    return QuicUint128Impl(0, 0);
  }
}

inline QuicUint128Impl& QuicUint128Impl::operator<<=(int amount) {
  // uint64_t shifts of >= 64 are undefined, so we will need some
  // special-casing.
  if (amount < 64) {
    if (amount != 0) {
      hi_ = (hi_ << amount) | (lo_ >> (64 - amount));
      lo_ = lo_ << amount;
    }
  } else if (amount < 128) {
    hi_ = lo_ << (amount - 64);
    lo_ = 0;
  } else {
    hi_ = 0;
    lo_ = 0;
  }
  return *this;
}

inline QuicUint128Impl& QuicUint128Impl::operator>>=(int amount) {
  // uint64_t shifts of >= 64 are undefined, so we will need some
  // special-casing.
  if (amount < 64) {
    if (amount != 0) {
      lo_ = (lo_ >> amount) | (hi_ << (64 - amount));
      hi_ = hi_ >> amount;
    }
  } else if (amount < 128) {
    hi_ = 0;
    lo_ = hi_ >> (amount - 64);
  } else {
    hi_ = 0;
    lo_ = 0;
  }
  return *this;
}

inline QuicUint128Impl operator+(const QuicUint128Impl& lhs,
                                 const QuicUint128Impl& rhs) {
  return QuicUint128Impl(lhs) += rhs;
}

inline QuicUint128Impl operator-(const QuicUint128Impl& lhs,
                                 const QuicUint128Impl& rhs) {
  return QuicUint128Impl(lhs) -= rhs;
}

inline QuicUint128Impl operator*(const QuicUint128Impl& lhs,
                                 const QuicUint128Impl& rhs) {
  return QuicUint128Impl(lhs) *= rhs;
}

inline QuicUint128Impl& QuicUint128Impl::operator+=(const QuicUint128Impl& b) {
  hi_ += b.hi_;
  uint64_t lolo = lo_ + b.lo_;
  if (lolo < lo_)
    ++hi_;
  lo_ = lolo;
  return *this;
}

inline QuicUint128Impl& QuicUint128Impl::operator-=(const QuicUint128Impl& b) {
  hi_ -= b.hi_;
  if (b.lo_ > lo_)
    --hi_;
  lo_ -= b.lo_;
  return *this;
}

inline QuicUint128Impl& QuicUint128Impl::operator*=(const QuicUint128Impl& b) {
  uint64_t a96 = hi_ >> 32;
  uint64_t a64 = hi_ & 0xffffffffu;
  uint64_t a32 = lo_ >> 32;
  uint64_t a00 = lo_ & 0xffffffffu;
  uint64_t b96 = b.hi_ >> 32;
  uint64_t b64 = b.hi_ & 0xffffffffu;
  uint64_t b32 = b.lo_ >> 32;
  uint64_t b00 = b.lo_ & 0xffffffffu;
  // multiply [a96 .. a00] x [b96 .. b00]
  // terms higher than c96 disappear off the high side
  // terms c96 and c64 are safe to ignore carry bit
  uint64_t c96 = a96 * b00 + a64 * b32 + a32 * b64 + a00 * b96;
  uint64_t c64 = a64 * b00 + a32 * b32 + a00 * b64;
  this->hi_ = (c96 << 32) + c64;
  this->lo_ = 0;
  // add terms after this one at a time to capture carry
  *this += QuicUint128Impl(a32 * b00) << 32;
  *this += QuicUint128Impl(a00 * b32) << 32;
  *this += a00 * b00;
  return *this;
}

inline QuicUint128Impl QuicUint128Impl::operator++(int) {
  QuicUint128Impl tmp(*this);
  *this += 1;
  return tmp;
}

inline QuicUint128Impl QuicUint128Impl::operator--(int) {
  QuicUint128Impl tmp(*this);
  *this -= 1;
  return tmp;
}

inline QuicUint128Impl& QuicUint128Impl::operator++() {
  *this += 1;
  return *this;
}

inline QuicUint128Impl& QuicUint128Impl::operator--() {
  *this -= 1;
  return *this;
}

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_UINT128_IMPL_H_
