/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/xml/xslt_unicode_sort.h"

#include <libxslt/templates.h>
#include <libxslt/xsltutils.h>
#include <memory>
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/i18n/unicode/ucol.h"

namespace blink {

namespace {

class UCollatorDeleter {
 public:
  void operator()(UCollator* collator) { ucol_close(collator); }
};

struct UCollatorHolder {
  std::unique_ptr<UCollator, UCollatorDeleter> collator;
  char equivalent_locale[ULOC_FULLNAME_CAPACITY];
  bool lower_first = false;

  operator UCollator*() const { return collator.get(); }
};

}  // namespace

inline const xmlChar* ToXMLChar(const char* string) {
  return reinterpret_cast<const xmlChar*>(string);
}

// Based on default implementation from libxslt 1.1.22 and xsltICUSort.c
// example.
void XsltUnicodeSortFunction(xsltTransformContextPtr ctxt,
                             xmlNodePtr* sorts,
                             int nbsorts) {
#ifdef XSLT_REFACTORED
  xsltStyleItemSortPtr comp;
#else
  xsltStylePreCompPtr comp;
#endif
  xmlXPathObjectPtr* results_tab[XSLT_MAX_SORT];
  xmlXPathObjectPtr* results = nullptr;
  xmlNodeSetPtr list = nullptr;
  int depth;
  xmlNodePtr node;
  int tempstype[XSLT_MAX_SORT], temporder[XSLT_MAX_SORT];

  if (!ctxt || !sorts || nbsorts <= 0 || nbsorts >= XSLT_MAX_SORT)
    return;
  if (!sorts[0])
    return;
  comp = static_cast<xsltStylePreComp*>(sorts[0]->psvi);
  if (!comp)
    return;

  list = ctxt->nodeList;
  if (!list || list->nodeNr <= 1)
    return;  // Nothing to do.

  for (int j = 0; j < nbsorts; ++j) {
    comp = static_cast<xsltStylePreComp*>(sorts[j]->psvi);
    tempstype[j] = 0;
    if (!comp->stype && comp->has_stype) {
      comp->stype = xsltEvalAttrValueTemplate(
          ctxt, sorts[j], ToXMLChar("data-type"), XSLT_NAMESPACE);
      if (comp->stype) {
        tempstype[j] = 1;
        if (xmlStrEqual(comp->stype, ToXMLChar("text"))) {
          comp->number = 0;
        } else if (xmlStrEqual(comp->stype, ToXMLChar("number"))) {
          comp->number = 1;
        } else {
          xsltTransformError(
              ctxt, nullptr, sorts[j],
              "xsltDoSortFunction: no support for data-type = %s\n",
              comp->stype);
          comp->number = 0;  // Use default.
        }
      }
    }
    temporder[j] = 0;
    if (!comp->order && comp->has_order) {
      comp->order = xsltEvalAttrValueTemplate(
          ctxt, sorts[j], ToXMLChar("order"), XSLT_NAMESPACE);
      if (comp->order) {
        temporder[j] = 1;
        if (xmlStrEqual(comp->order, ToXMLChar("ascending"))) {
          comp->descending = 0;
        } else if (xmlStrEqual(comp->order, ToXMLChar("descending"))) {
          comp->descending = 1;
        } else {
          xsltTransformError(ctxt, nullptr, sorts[j],
                             "xsltDoSortFunction: invalid value %s for order\n",
                             comp->order);
          comp->descending = 0;  // Use default.
        }
      }
    }
  }

  int len = list->nodeNr;

  results_tab[0] = xsltComputeSortResult(ctxt, sorts[0]);
  for (int i = 1; i < XSLT_MAX_SORT; ++i)
    results_tab[i] = nullptr;

  results = results_tab[0];

  comp = static_cast<xsltStylePreComp*>(sorts[0]->psvi);
  int descending = comp->descending;
  int number = comp->number;
  if (!results)
    return;

  // We are passing a language identifier to a function that expects a locale
  // identifier. The implementation of Collator should be lenient, and accept
  // both "en-US" and "en_US", for example. This lets an author to really
  // specify sorting rules, e.g. "de_DE@collation=phonebook", which isn't
  // possible with language alone.
  const char* lang =
      comp->has_lang ? reinterpret_cast<const char*>(comp->lang) : "en";

  UErrorCode status = U_ZERO_ERROR;
  char equivalent_locale[ULOC_FULLNAME_CAPACITY];
  UBool is_available;
  ucol_getFunctionalEquivalent(equivalent_locale, ULOC_FULLNAME_CAPACITY,
                               "collation", lang, &is_available, &status);
  if (U_FAILURE(status)) {
    strcpy(equivalent_locale, "root");
    status = U_ZERO_ERROR;
  }

  DEFINE_STATIC_LOCAL(std::unique_ptr<UCollatorHolder>, cached_collator, ());
  std::unique_ptr<UCollatorHolder> collator;
  if (cached_collator &&
      !strcmp(cached_collator->equivalent_locale, equivalent_locale) &&
      cached_collator->lower_first == comp->lower_first) {
    collator = std::move(cached_collator);
  } else {
    collator = std::make_unique<UCollatorHolder>();
    strncpy(collator->equivalent_locale, equivalent_locale,
            ULOC_FULLNAME_CAPACITY);
    collator->lower_first = comp->lower_first;

    collator->collator.reset(ucol_open(lang, &status));
    if (U_FAILURE(status)) {
      status = U_ZERO_ERROR;
      collator->collator.reset(ucol_open("", &status));
    }
    DCHECK(U_SUCCESS(status));
    ucol_setAttribute(*collator, UCOL_CASE_FIRST,
                      comp->lower_first ? UCOL_LOWER_FIRST : UCOL_UPPER_FIRST,
                      &status);
    DCHECK(U_SUCCESS(status));
    ucol_setAttribute(*collator, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    DCHECK(U_SUCCESS(status));
  }

  // Shell's sort of node-set.
  for (int incr = len / 2; incr > 0; incr /= 2) {
    for (int i = incr; i < len; ++i) {
      int j = i - incr;
      if (!results[i])
        continue;

      while (j >= 0) {
        int tst;
        if (!results[j]) {
          tst = 1;
        } else {
          if (number) {
            // We make NaN smaller than number in accordance with
            // XSLT spec.
            if (xmlXPathIsNaN(results[j]->floatval)) {
              if (xmlXPathIsNaN(results[j + incr]->floatval))
                tst = 0;
              else
                tst = -1;
            } else if (xmlXPathIsNaN(results[j + incr]->floatval)) {
              tst = 1;
            } else if (results[j]->floatval == results[j + incr]->floatval) {
              tst = 0;
            } else if (results[j]->floatval > results[j + incr]->floatval) {
              tst = 1;
            } else {
              tst = -1;
            }
          } else {
            Vector<UChar> string1;
            Vector<UChar> string2;
            String::FromUTF8(
                reinterpret_cast<const char*>(results[j]->stringval))
                .AppendTo(string1);
            String::FromUTF8(
                reinterpret_cast<const char*>(results[j + incr]->stringval))
                .AppendTo(string2);
            tst = ucol_strcoll(*collator, string1.data(), string1.size(),
                               string2.data(), string2.size());
          }
          if (descending)
            tst = -tst;
        }
        if (tst == 0) {
          // Okay we need to use multi level sorts.
          depth = 1;
          while (depth < nbsorts) {
            if (!sorts[depth])
              break;
            comp = static_cast<xsltStylePreComp*>(sorts[depth]->psvi);
            if (!comp)
              break;
            int desc = comp->descending;
            int numb = comp->number;

            // Compute the result of the next level for the full
            // set, this might be optimized ... or not
            if (!results_tab[depth])
              results_tab[depth] = xsltComputeSortResult(ctxt, sorts[depth]);
            xmlXPathObjectPtr* res = results_tab[depth];
            if (!res)
              break;
            if (!res[j]) {
              if (res[j + incr])
                tst = 1;
            } else {
              if (numb) {
                // We make NaN smaller than number in accordance
                // with XSLT spec.
                if (xmlXPathIsNaN(res[j]->floatval)) {
                  if (xmlXPathIsNaN(res[j + incr]->floatval))
                    tst = 0;
                  else
                    tst = -1;
                } else if (xmlXPathIsNaN(res[j + incr]->floatval)) {
                  tst = 1;
                } else if (res[j]->floatval == res[j + incr]->floatval) {
                  tst = 0;
                } else if (res[j]->floatval > res[j + incr]->floatval) {
                  tst = 1;
                } else {
                  tst = -1;
                }
              } else {
                Vector<UChar> string1;
                Vector<UChar> string2;
                String::FromUTF8(
                    reinterpret_cast<const char*>(res[j]->stringval))
                    .AppendTo(string1);
                String::FromUTF8(
                    reinterpret_cast<const char*>(res[j + incr]->stringval))
                    .AppendTo(string2);
                tst = ucol_strcoll(*collator, string1.data(), string1.size(),
                                   string2.data(), string2.size());
              }
              if (desc)
                tst = -tst;
            }

            // if we still can't differenciate at this level try one
            // level deeper.
            if (tst != 0)
              break;
            depth++;
          }
        }
        if (tst == 0) {
          tst = results[j]->index > results[j + incr]->index;
        }
        if (tst > 0) {
          xmlXPathObjectPtr tmp = results[j];
          results[j] = results[j + incr];
          results[j + incr] = tmp;
          node = list->nodeTab[j];
          list->nodeTab[j] = list->nodeTab[j + incr];
          list->nodeTab[j + incr] = node;
          depth = 1;
          while (depth < nbsorts) {
            if (!sorts[depth])
              break;
            if (!results_tab[depth])
              break;
            xmlXPathObjectPtr* res = results_tab[depth];
            tmp = res[j];
            res[j] = res[j + incr];
            res[j + incr] = tmp;
            depth++;
          }
          j -= incr;
        } else {
          break;
        }
      }
    }
  }

  for (int j = 0; j < nbsorts; ++j) {
    comp = static_cast<xsltStylePreComp*>(sorts[j]->psvi);
    if (tempstype[j] == 1) {
      // The data-type needs to be recomputed each time.
      xmlFree(const_cast<xmlChar*>(comp->stype));
      comp->stype = nullptr;
    }
    if (temporder[j] == 1) {
      // The order needs to be recomputed each time.
      xmlFree(const_cast<xmlChar*>(comp->order));
      comp->order = nullptr;
    }
    if (results_tab[j]) {
      for (int i = 0; i < len; ++i)
        xmlXPathFreeObject(results_tab[j][i]);
      xmlFree(results_tab[j]);
    }
  }

  cached_collator = std::move(collator);
}

}  // namespace blink
