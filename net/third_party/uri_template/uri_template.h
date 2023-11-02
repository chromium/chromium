/*
 * \copyright Copyright 2013 Google Inc. All Rights Reserved.
 * \license @{
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @}
 */

#ifndef NET_THIRD_PARTY_URI_TEMPLATE_URI_TEMPLATE_H_
#define NET_THIRD_PARTY_URI_TEMPLATE_URI_TEMPLATE_H_

#include <set>
#include <string>
#include <unordered_map>

#include "base/component_export.h"

using std::string;

namespace uri_template {

/*
 * Produce concrete URLs from templated ones. Collect the names of any expanded
 * variables. Supports templates up to level 3 as specified in RFC 6570 with
 * some limitations: it does not check for disallowed characters in variable
 * names, and it does not do any encoding during literal expansion.
 *
 * @param[in] uri The templated uri to expand.
 * @param[in] parameters A map containing variables and corresponding values.
 * @param[out] target The string to resolve the templated uri into.
 * @param[out] vars_found Populated with the list of variables names
 *             that were resolved while expanding the uri. NULL is permitted
 *             if the caller does not wish to collect these.
 *
 * @return true if the template was parseable. false if it was malformed.
 */
COMPONENT_EXPORT(URI_TEMPLATE)
bool Expand(const string& template_uri,
            const std::unordered_map<string, string>& parameters,
            string* target,
            std::set<string>* vars_found = nullptr);

}  // namespace uri_template

#endif  // NET_THIRD_PARTY_URI_TEMPLATE_URI_TEMPLATE_H_
