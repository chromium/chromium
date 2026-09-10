/*
 * Copyright (C) 2005, 2006, 2008, 2010, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_FUNCTIONS_MEMORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_FUNCTIONS_MEMORY_H_

#include <stdint.h>

#include <concepts>

#include "base/containers/span.h"
#include "third_party/rapidhash/rapidhash.h"

namespace blink {

// TODO(crbug.com/458429790): Once clang is better able to optimize this,
// simplify this to a single overload that accepts a base::span<const
// uint8_t>.
template <typename T>
  requires(std::convertible_to<T, base::span<const uint8_t>>)
inline uint64_t HashMemory64(const T& t) {
  base::span data = t;
  static_assert(std::same_as<typename decltype(data)::value_type, uint8_t>);
  return rapidhash(data.data(), data.size());
}

// TODO(crbug.com/458429790): Once clang is better able to optimize this,
// simplify this to a single overload that accepts a base::span<const
// uint8_t>.
template <typename T>
  requires(std::convertible_to<T, base::span<const uint8_t>>)
inline uint32_t HashMemory32(const T& t) {
  return static_cast<uint32_t>(HashMemory64(t));
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_FUNCTIONS_MEMORY_H_
