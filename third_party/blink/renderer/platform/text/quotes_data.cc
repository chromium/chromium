/**
 * Copyright (C) 2011 Nokia Inc.  All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/text/quotes_data.h"

namespace blink {

scoped_refptr<QuotesData> QuotesData::Create(UChar open1,
                                             UChar close1,
                                             UChar open2,
                                             UChar close2) {
  scoped_refptr<QuotesData> data = QuotesData::Create();
  data->AddPair(std::make_pair(String(&open1, 1u), String(&close1, 1u)));
  data->AddPair(std::make_pair(String(&open2, 1u), String(&close2, 1u)));
  return data;
}

void QuotesData::AddPair(std::pair<String, String> quote_pair) {
  quote_pairs_.push_back(quote_pair);
}

const String QuotesData::GetOpenQuote(int index) const {
  DCHECK_GE(index, 0);
  if (!quote_pairs_.size() || index < 0)
    return g_empty_string;
  if ((size_t)index >= quote_pairs_.size())
    return quote_pairs_.back().first;
  return quote_pairs_.at(index).first;
}

const String QuotesData::GetCloseQuote(int index) const {
  DCHECK_GE(index, -1);
  if (!quote_pairs_.size() || index < 0)
    return g_empty_string;
  if ((size_t)index >= quote_pairs_.size())
    return quote_pairs_.back().second;
  return quote_pairs_.at(index).second;
}

}  // namespace blink
