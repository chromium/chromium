// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_PAC_JS_LIBRARY_H_
#define SERVICES_PROXY_RESOLVER_PAC_JS_LIBRARY_H_

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Akhil Arora <akhil.arora@sun.com>
 *   Tomi Leppikangas <Tomi.Leppikangas@oulu.fi>
 *   Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// The following code was last extracted from netwerk/base/ProxyAutoConfig.cpp
// on 2018-03-29 using this command:
//
// REV="6aa3b57955fed5e137d0306478e1a4b424a6d392"
// FILE_PATH="netwerk/base/ProxyAutoConfig.cpp"
// URL="https://hg.mozilla.org/mozilla-central/raw-file/$REV/$FILE_PATH"
//
// curl "$URL" | awk '/sPacUtils =/,/ "";/' | sed -e 's/"$/" \\/g'
//
// Additionally, the definition for isPlainHostName() was removed, as it is
// implemented by the C++ side already.
#define PAC_JS_LIBRARY                                                         \
  "function dnsDomainIs(host, domain) {\n"                                     \
  "    return (host.length >= domain.length &&\n"                              \
  "            host.substring(host.length - domain.length) == domain);\n"      \
  "}\n"                                                                        \
  ""                                                                           \
  "function dnsDomainLevels(host) {\n"                                         \
  "    return host.split('.').length - 1;\n"                                   \
  "}\n"                                                                        \
  ""                                                                           \
  "function isValidIpAddress(ipchars) {\n"                                     \
  "    var matches = "                                                         \
  "/^(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})$/.exec(ipchars);\n"     \
  "    if (matches == null) {\n"                                               \
  "        return false;\n"                                                    \
  "    } else if (matches[1] > 255 || matches[2] > 255 || \n"                  \
  "               matches[3] > 255 || matches[4] > 255) {\n"                   \
  "        return false;\n"                                                    \
  "    }\n"                                                                    \
  "    return true;\n"                                                         \
  "}\n"                                                                        \
  ""                                                                           \
  "function convert_addr(ipchars) {\n"                                         \
  "    var bytes = ipchars.split('.');\n"                                      \
  "    var result = ((bytes[0] & 0xff) << 24) |\n"                             \
  "                 ((bytes[1] & 0xff) << 16) |\n"                             \
  "                 ((bytes[2] & 0xff) <<  8) |\n"                             \
  "                  (bytes[3] & 0xff);\n"                                     \
  "    return result;\n"                                                       \
  "}\n"                                                                        \
  ""                                                                           \
  "function isInNet(ipaddr, pattern, maskstr) {\n"                             \
  "    if (!isValidIpAddress(pattern) || !isValidIpAddress(maskstr)) {\n"      \
  "        return false;\n"                                                    \
  "    }\n"                                                                    \
  "    if (!isValidIpAddress(ipaddr)) {\n"                                     \
  "        ipaddr = dnsResolve(ipaddr);\n"                                     \
  "        if (ipaddr == null) {\n"                                            \
  "            return false;\n"                                                \
  "        }\n"                                                                \
  "    }\n"                                                                    \
  "    var host = convert_addr(ipaddr);\n"                                     \
  "    var pat  = convert_addr(pattern);\n"                                    \
  "    var mask = convert_addr(maskstr);\n"                                    \
  "    return ((host & mask) == (pat & mask));\n"                              \
  "    \n"                                                                     \
  "}\n"                                                                        \
  ""                                                                           \
  "function isResolvable(host) {\n"                                            \
  "    var ip = dnsResolve(host);\n"                                           \
  "    return (ip != null);\n"                                                 \
  "}\n"                                                                        \
  ""                                                                           \
  "function localHostOrDomainIs(host, hostdom) {\n"                            \
  "    return (host == hostdom) ||\n"                                          \
  "           (hostdom.lastIndexOf(host + '.', 0) == 0);\n"                    \
  "}\n"                                                                        \
  ""                                                                           \
  "function shExpMatch(url, pattern) {\n"                                      \
  "   pattern = pattern.replace(/\\./g, '\\\\.');\n"                           \
  "   pattern = pattern.replace(/\\*/g, '.*');\n"                              \
  "   pattern = pattern.replace(/\\?/g, '.');\n"                               \
  "   var newRe = new RegExp('^'+pattern+'$');\n"                              \
  "   return newRe.test(url);\n"                                               \
  "}\n"                                                                        \
  ""                                                                           \
  "var wdays = {SUN: 0, MON: 1, TUE: 2, WED: 3, THU: 4, FRI: 5, SAT: 6};\n"    \
  "var months = {JAN: 0, FEB: 1, MAR: 2, APR: 3, MAY: 4, JUN: 5, JUL: 6, "     \
  "AUG: 7, SEP: 8, OCT: 9, NOV: 10, DEC: 11};\n"                               \
  ""                                                                           \
  "function weekdayRange() {\n"                                                \
  "    function getDay(weekday) {\n"                                           \
  "        if (weekday in wdays) {\n"                                          \
  "            return wdays[weekday];\n"                                       \
  "        }\n"                                                                \
  "        return -1;\n"                                                       \
  "    }\n"                                                                    \
  "    var date = new Date();\n"                                               \
  "    var argc = arguments.length;\n"                                         \
  "    var wday;\n"                                                            \
  "    if (argc < 1)\n"                                                        \
  "        return false;\n"                                                    \
  "    if (arguments[argc - 1] == 'GMT') {\n"                                  \
  "        argc--;\n"                                                          \
  "        wday = date.getUTCDay();\n"                                         \
  "    } else {\n"                                                             \
  "        wday = date.getDay();\n"                                            \
  "    }\n"                                                                    \
  "    var wd1 = getDay(arguments[0]);\n"                                      \
  "    var wd2 = (argc == 2) ? getDay(arguments[1]) : wd1;\n"                  \
  "    return (wd1 == -1 || wd2 == -1) ? false\n"                              \
  "                                    : (wd1 <= wd2) ? (wd1 <= wday && wday " \
  "<= wd2)\n"                                                                  \
  "                                                   : (wd2 >= wday || wday " \
  ">= wd1);\n"                                                                 \
  "}\n"                                                                        \
  ""                                                                           \
  "function dateRange() {\n"                                                   \
  "    function getMonth(name) {\n"                                            \
  "        if (name in months) {\n"                                            \
  "            return months[name];\n"                                         \
  "        }\n"                                                                \
  "        return -1;\n"                                                       \
  "    }\n"                                                                    \
  "    var date = new Date();\n"                                               \
  "    var argc = arguments.length;\n"                                         \
  "    if (argc < 1) {\n"                                                      \
  "        return false;\n"                                                    \
  "    }\n"                                                                    \
  "    var isGMT = (arguments[argc - 1] == 'GMT');\n"                          \
  "\n"                                                                         \
  "    if (isGMT) {\n"                                                         \
  "        argc--;\n"                                                          \
  "    }\n"                                                                    \
  "    // function will work even without explict handling of this case\n"     \
  "    if (argc == 1) {\n"                                                     \
  "        var tmp = parseInt(arguments[0]);\n"                                \
  "        if (isNaN(tmp)) {\n"                                                \
  "            return ((isGMT ? date.getUTCMonth() : date.getMonth()) ==\n"    \
  "                     getMonth(arguments[0]));\n"                            \
  "        } else if (tmp < 32) {\n"                                           \
  "            return ((isGMT ? date.getUTCDate() : date.getDate()) == "       \
  "tmp);\n"                                                                    \
  "        } else { \n"                                                        \
  "            return ((isGMT ? date.getUTCFullYear() : date.getFullYear()) "  \
  "==\n"                                                                       \
  "                     tmp);\n"                                               \
  "        }\n"                                                                \
  "    }\n"                                                                    \
  "    var year = date.getFullYear();\n"                                       \
  "    var date1, date2;\n"                                                    \
  "    date1 = new Date(year,  0,  1,  0,  0,  0);\n"                          \
  "    date2 = new Date(year, 11, 31, 23, 59, 59);\n"                          \
  "    var adjustMonth = false;\n"                                             \
  "    for (var i = 0; i < (argc >> 1); i++) {\n"                              \
  "        var tmp = parseInt(arguments[i]);\n"                                \
  "        if (isNaN(tmp)) {\n"                                                \
  "            var mon = getMonth(arguments[i]);\n"                            \
  "            date1.setMonth(mon);\n"                                         \
  "        } else if (tmp < 32) {\n"                                           \
  "            adjustMonth = (argc <= 2);\n"                                   \
  "            date1.setDate(tmp);\n"                                          \
  "        } else {\n"                                                         \
  "            date1.setFullYear(tmp);\n"                                      \
  "        }\n"                                                                \
  "    }\n"                                                                    \
  "    for (var i = (argc >> 1); i < argc; i++) {\n"                           \
  "        var tmp = parseInt(arguments[i]);\n"                                \
  "        if (isNaN(tmp)) {\n"                                                \
  "            var mon = getMonth(arguments[i]);\n"                            \
  "            date2.setMonth(mon);\n"                                         \
  "        } else if (tmp < 32) {\n"                                           \
  "            date2.setDate(tmp);\n"                                          \
  "        } else {\n"                                                         \
  "            date2.setFullYear(tmp);\n"                                      \
  "        }\n"                                                                \
  "    }\n"                                                                    \
  "    if (adjustMonth) {\n"                                                   \
  "        date1.setMonth(date.getMonth());\n"                                 \
  "        date2.setMonth(date.getMonth());\n"                                 \
  "    }\n"                                                                    \
  "    if (isGMT) {\n"                                                         \
  "    var tmp = date;\n"                                                      \
  "        tmp.setFullYear(date.getUTCFullYear());\n"                          \
  "        tmp.setMonth(date.getUTCMonth());\n"                                \
  "        tmp.setDate(date.getUTCDate());\n"                                  \
  "        tmp.setHours(date.getUTCHours());\n"                                \
  "        tmp.setMinutes(date.getUTCMinutes());\n"                            \
  "        tmp.setSeconds(date.getUTCSeconds());\n"                            \
  "        date = tmp;\n"                                                      \
  "    }\n"                                                                    \
  "    return (date1 <= date2) ? (date1 <= date) && (date <= date2)\n"         \
  "                            : (date2 >= date) || (date >= date1);\n"        \
  "}\n"                                                                        \
  ""                                                                           \
  "function timeRange() {\n"                                                   \
  "    var argc = arguments.length;\n"                                         \
  "    var date = new Date();\n"                                               \
  "    var isGMT= false;\n"                                                    \
  ""                                                                           \
  "    if (argc < 1) {\n"                                                      \
  "        return false;\n"                                                    \
  "    }\n"                                                                    \
  "    if (arguments[argc - 1] == 'GMT') {\n"                                  \
  "        isGMT = true;\n"                                                    \
  "        argc--;\n"                                                          \
  "    }\n"                                                                    \
  "\n"                                                                         \
  "    var hour = isGMT ? date.getUTCHours() : date.getHours();\n"             \
  "    var date1, date2;\n"                                                    \
  "    date1 = new Date();\n"                                                  \
  "    date2 = new Date();\n"                                                  \
  "\n"                                                                         \
  "    if (argc == 1) {\n"                                                     \
  "        return (hour == arguments[0]);\n"                                   \
  "    } else if (argc == 2) {\n"                                              \
  "        return ((arguments[0] <= hour) && (hour <= arguments[1]));\n"       \
  "    } else {\n"                                                             \
  "        switch (argc) {\n"                                                  \
  "        case 6:\n"                                                          \
  "            date1.setSeconds(arguments[2]);\n"                              \
  "            date2.setSeconds(arguments[5]);\n"                              \
  "        case 4:\n"                                                          \
  "            var middle = argc >> 1;\n"                                      \
  "            date1.setHours(arguments[0]);\n"                                \
  "            date1.setMinutes(arguments[1]);\n"                              \
  "            date2.setHours(arguments[middle]);\n"                           \
  "            date2.setMinutes(arguments[middle + 1]);\n"                     \
  "            if (middle == 2) {\n"                                           \
  "                date2.setSeconds(59);\n"                                    \
  "            }\n"                                                            \
  "            break;\n"                                                       \
  "        default:\n"                                                         \
  "          throw 'timeRange: bad number of arguments'\n"                     \
  "        }\n"                                                                \
  "    }\n"                                                                    \
  "\n"                                                                         \
  "    if (isGMT) {\n"                                                         \
  "        date.setFullYear(date.getUTCFullYear());\n"                         \
  "        date.setMonth(date.getUTCMonth());\n"                               \
  "        date.setDate(date.getUTCDate());\n"                                 \
  "        date.setHours(date.getUTCHours());\n"                               \
  "        date.setMinutes(date.getUTCMinutes());\n"                           \
  "        date.setSeconds(date.getUTCSeconds());\n"                           \
  "    }\n"                                                                    \
  "    return (date1 <= date2) ? (date1 <= date) && (date <= date2)\n"         \
  "                            : (date2 >= date) || (date >= date1);\n"        \
  "\n"                                                                         \
  "}\n"

// This is a Microsoft extension to PAC for IPv6, see:
// http://blogs.msdn.com/b/wndp/archive/2006/07/13/ipv6-pac-extensions-v0-9.aspx
#define PAC_JS_LIBRARY_EX                  \
  "function isResolvableEx(host) {\n"      \
  "    var ipList = dnsResolveEx(host);\n" \
  "    return (ipList != '');\n"           \
  "}\n"

#endif  // SERVICES_PROXY_RESOLVER_PAC_JS_LIBRARY_H_
