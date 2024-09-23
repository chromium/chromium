// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/annotations/ax_image_stopwords.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/icu/source/common/unicode/uchar.h"

namespace content {

namespace {

// List of image stopwords for all languages. See ax_image_stopwords.h
// for information about how image stopwords are defined and how they're
// used.
//
// The stopwords are encoded here as a single long string delimited by
// newlines. This is much more efficient than an array of strings, which
// in practice takes ~6x more storage in the resulting binary.
//
// Current size as of June 2020:
//   369 unique words
//   2542 bytes uncompressed
//   1127 bytes gzipped
const char kImageStopwordsUtf8[] = {
    //
    // Language-independent stopwords.
    //

    "com\n"
    "edu\n"
    "http\n"
    "https\n"
    "www\n"

    //
    // English.
    //

    // General English stopwords.
    "and\n"
    "are\n"
    "for\n"
    "from\n"
    "how\n"
    "online\n"
    "our\n"
    "the\n"
    "this\n"
    "with\n"
    "you\n"
    "your\n"

    // English image-specific stopwords.
    "art\n"
    "avatar\n"
    "background\n"
    "backgrounds\n"
    "black\n"
    "download\n"
    "drawing\n"
    "drawings\n"
    "free\n"
    "gif\n"
    "icon\n"
    "icons\n"
    "illustration\n"
    "illustrations\n"
    "image\n"
    "images\n"
    "jpeg\n"
    "jpg\n"
    "logo\n"
    "logos\n"
    "meme\n"
    "memes\n"
    "photo\n"
    "photos\n"
    "picture\n"
    "pictures\n"
    "png\n"
    "stock\n"
    "transparent\n"
    "vector\n"
    "vectors\n"
    "video\n"
    "videos\n"
    "wallpaper\n"
    "wallpapers\n"
    "white\n"

    // Alt text from may images starts with "Image may contain".
    "may\n"
    "contain\n"

    // Many images on the web have the alt text "Highlights info row."
    "highlights\n"
    "info\n"
    "row\n"

    // Google Photos images are labeled as "portrait" or "landscape".
    "portrait\n"
    "landscape\n"

    // Reddit says "post image".
    "post\n"

    // Months and month abbreviations. Often used as part of the date/time
    // when a photograph was taken.
    "january\n"
    "jan\n"
    "february\n"
    "feb\n"
    "march\n"
    "mar\n"
    "april\n"
    "apr\n"
    "may\n"
    "june\n"
    "jun\n"
    "july\n"
    "jul\n"
    "august\n"
    "aug\n"
    "september\n"
    "sep\n"
    "october\n"
    "oct"
    "november"
    "nov\n"
    "december\n"
    "dec\n"

    // Days of the week.
    "monday\n"
    "mon\n"
    "tuesday\n"
    "tue\n"
    "wednesday\n"
    "wed\n"
    "thursday\n"
    "thu\n"
    "friday\n"
    "fri\n"
    "saturday\n"
    "sat\n"
    "sunday\n"
    "sun\n"

    //
    // French
    //

    // General French stopwords.
    "les\n"   // the
    "pour\n"  // for
    "des\n"   // of the
    "sur\n"   // on
    "avec\n"  // with
    "une\n"   // one
    "dans\n"  // in
    "est\n"   // is
    "plus\n"  // more
    "par\n"   // by
    "vous\n"  // you
    "pas\n"   // not
    "qui\n"   // who
    "aux\n"   // to the
    "son\n"   // his/her/its
    "nous\n"  // we
    "voir\n"  // see

    // French Image stopwords.
    "noir\n"      // black
    "blanc\n"     // white
    "dessin\n"    // drawing
    "font\n"      // background
    "peinture\n"  // painting

    // Months.
    "janvier\n"
    "janv\n"
    "février\n"
    "févr\n"
    "mars\n"
    "avril\n"
    "mai\n"
    "juin\n"
    "juillet\n"
    "juil\n"
    "août\n"
    "septembre\n"
    "sept\n"
    "octobre\n"
    "oct\n"
    "novembre\n"
    "nov\n"
    "décembre\n"
    "déc\n"

    // Days of the week.
    "lundi\n"
    "lun\n"
    "mardi\n"
    "mar\n"
    "mercredi\n"
    "mer\n"
    "jeudi\n"
    "jeu\n"
    "vendredi\n"
    "ven\n"
    "samedi\n"
    "sam\n"
    "dimanche\n"
    "dim\n"

    //
    // Italian
    //

    "con\n"    // with
    "per\n"    // through, by
    "non\n"    // not
    "come\n"   // as
    "più\n"    // more
    "dal\n"    // da + il
    "dallo\n"  // da + lo
    "dai\n"    // da + i
    "dagli\n"  // da + gli
    "dall\n"   // da + l'
    "dagl\n"   // da + gll'
    "dalla\n"  // da + la
    "dalle\n"  // da + le
    "del\n"    // di + il
    "dello\n"  // di + lo
    "dei\n"    // di + i
    "degli\n"  // di + gli
    "dell\n"   // di + l'
    "degl\n"   // di + gl'
    "della\n"  // di + la
    "delle\n"  // di + le
    "nel\n"    // in + el
    "nello\n"  // in + lo
    "nei\n"    // in + i
    "negli\n"  // in + gli
    "nell\n"   // in + l'
    "negl\n"   // in + gl'
    "nella\n"  // in + la
    "nelle\n"  // in + le
    "sul\n"    // su + il
    "sullo\n"  // su + lo
    "sui\n"    // su + i
    "sugli\n"  // su + gli
    "sull\n"   // su + l'
    "sugl\n"   // su + gl'
    "sulla\n"  // su + la
    "sulle\n"  // su + le

    // Images
    "arte\n"
    "immagini\n"
    "illustrazione\n"
    "fotografia\n"
    "icona\n"

    "bianca\n"     // white
    "bianco\n"     // white
    "nera\n"       // black
    "nero\n"       // black
    "contenere\n"  // contain (image may contain...)

    // Months.
    "gennaio\n"
    "genn\n"
    "febbraio\n"
    "febbr\n"
    "marzo\n"
    "mar\n"
    "aprile\n"
    "apr\n"
    "maggio\n"
    "magg\n"
    "giugno\n"
    "luglio\n"
    "agosto\n"
    "settembre\n"
    "sett\n"
    "ottobre\n"
    "ott\n"
    "novembre\n"
    "nov\n"
    "dicembre\n"
    "dic\n"

    // Weekdays.
    "lunedì\n"
    "lun\n"
    "martedì\n"
    "mar\n"
    "mercoledì\n"
    "mer\n"
    "giovedì\n"
    "gio\n"
    "venerdì\n"
    "ven\n"
    "sabato\n"
    "sab\n"
    "domenica\n"
    "dom\n"

    //
    // German
    //

    // General German stopwords.
    "und\n"      // and
    "mit\n"      // with
    "für\n"      // for
    "der\n"      // the
    "die\n"      // the
    "von\n"      // of, from
    "auf\n"      // on
    "das\n"      // the
    "aus\n"      // out of
    "ist\n"      // is
    "ein\n"      // one
    "eine\n"     // one
    "sie\n"      // they, she
    "den\n"      // the
    "zum\n"      // zu + dem
    "zur\n"      // zu + der
    "bei\n"      // by
    "des\n"      // the
    "sprüche\n"  // claims (to be)
    "oder\n"     // or

    // German Image stopwords.
    "bild\n"     // picture
    "bilder\n"   // pictures
    "foto\n"     // photo
    "schwarz\n"  // black
    "weiß\n"     // white

    // Months.
    "januar\n"
    "jan\n"
    "jän\n"
    "februar\n"
    "feb\n"
    "märz\n"
    "april\n"
    "apr\n"
    "mai\n"
    "juni\n"
    "juli\n"
    "august\n"
    "aug\n"
    "september\n"
    "sept\n"
    "oktober\n"
    "okt\n"
    "november\n"
    "nov\n"
    "dezember\n"
    "dez\n"

    // Weekdays.
    "montag\n"
    "dienstag\n"
    "mittwoch\n"
    "donnerstag\n"
    "freitag\n"
    "samstag\n"
    "sonntag\n"

    //
    // Spanish
    //

    // General Spanish stopwords.
    "con\n"   // with
    "para\n"  // by
    "del\n"   // of the
    "que\n"   // that
    "los\n"   // the
    "las\n"   // the
    "una\n"   // one
    "por\n"   // for
    "más\n"   // more
    "como\n"  // how

    // Spanish image stopwords.
    "dibujos\n"      // drawings
    "imagen\n"       // images
    "arte\n"         // art
    "fondo\n"        // background
    "fondos\n"       // backgrounds
    "diseño\n"       // design
    "ilustración\n"  // illustration
    "imagenes\n"     // images
    "blanca\n"       // white
    "blanco\n"       // white
    "negra\n"        // black
    "negro\n"        // black

    // Months.
    "enero\n"
    "febrero\n"
    "feb\n"
    "marzo\n"
    "abril\n"
    "abr\n"
    "mayo\n"
    "junio\n"
    "jun\n"
    "julio\n"
    "jul\n"
    "agosto\n"
    "septiembre\n"
    "sept\n"
    "set\n"
    "octubre\n"
    "oct\n"
    "noviembre\n"
    "nov\n"
    "diciembre\n"
    "dic\n"

    // Weekdays. Weekday abbreviations in Spanish are two-letters which
    // don't need to be listed here (anything less than 3 letters is
    // considered a stopword already).
    "lunes\n"
    "martes\n"
    "miércoles\n"
    "jueves\n"
    "viernes\n"
    "sábado\n"
    "domingo\n"

    //
    // Hindi
    //

    // General Hindi stopwords.
    "में\n"      // in
    "लिए\n"    // for
    "नहीं\n"    // no
    "एक\n"     // one
    "साथ\n"    // with
    "दिया\n"   // gave
    "किया\n"   // did
    "रहे\n"     // are
    "सकता\n"   // can
    "इस\n"     // this
    "शामिल\n"  // include
    "तारीख\n"  // the date

    // Hindi image stopwords.
    "चित्र\n"    // picture
    "वीडियो\n"  // video

    // Months
    "जनवरी\n"
    "फरवरी\n"
    "मार्च\n"
    "अप्रैल\n"
    "मई\n"
    "जून\n"
    "जुलाई\n"
    "अगस्त\n"
    "सितंबर\n"
    "अक्टूबर\n"
    "नवंबर\n"
    "दिसंबर\n"

    // Weekdays.
    "सोमवार\n"
    "मंगलवार\n"
    "बुधवार\n"
    "बृहस्पतिवार\n"
    "शुक्रवार\n"
    "शनिवार\n"
    "रविवार\n"};

}  // namespace

// static
AXImageStopwords& AXImageStopwords::GetInstance() {
  static base::NoDestructor<AXImageStopwords> instance;
  return *instance;
}

AXImageStopwords::AXImageStopwords() {
  // Parse the newline-delimited stopwords from kImageStopwordsUtf8 and store
  // them as a flat_set of type string_view. This is very memory-efficient
  // because it avoids ever needing to copy any of the strings; each
  // string_view is just a pointer into kImageStopwordsUtf8 and flat_set
  // acts like a set but basically just does a binary search.
  std::vector<std::string_view> stopwords =
      base::SplitStringPiece(kImageStopwordsUtf8, "\n", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);

  // It's inefficient to add things to a flat_set one at a time. Copy them
  // all over at once.
  stopword_set_ = stopwords;
}

AXImageStopwords::~AXImageStopwords() = default;

bool AXImageStopwords::IsImageStopword(const char* word_utf8) const {
  std::u16string word_utf16 = base::UTF8ToUTF16(word_utf8);

  // It's not really meaningful, but since short words are stopwords, for
  // simplicity we define the empty string to be a stopword too.
  if (word_utf16.empty())
    return true;

  // Canonicalize case, this is like "ToLower" for many languages but
  // works independently of the current locale.
  word_utf16 = base::i18n::FoldCase(word_utf16);

  // Count the number of distinct codepoints from a supported unicode block.
  int supported_count = 0;
  base::i18n::UTF16CharIterator iter(word_utf16);
  for (; !iter.end(); iter.Advance()) {
    int32_t codepoint = iter.get();
    UBlockCode block_code = ublock_getCode(codepoint);
    switch (block_code) {
      case UBLOCK_BASIC_LATIN:
      case UBLOCK_LATIN_1_SUPPLEMENT:
      case UBLOCK_LATIN_EXTENDED_A:
      case UBLOCK_DEVANAGARI:
        supported_count++;
        break;
      default:
        break;
    }
  }

  // Treat any string of 2 or fewer characters in any of these unicode
  // blocks as a stopword. Of course, there are rare exceptions, but they're
  // acceptable for this heuristic. All that means is that we won't count
  // those words when trying to determine if the alt text for an image
  // has meaningful text or not. The odds are good that a 1 or 2 character
  // word is not meaningful.
  //
  // Note: this assumption might not be valid for some unicode blocks,
  // like Chinese. That's why the heuristic only applies to certain unicode
  // blocks where we believe this to be a reasonable assumption.
  //
  // Note that in Devanagari (the script used for the Hindi language, among
  // others) a word sometimes looks like a single character (one glyph) but it's
  // actually two or more unicode codepoints (consonants and vowels that are
  // joined together), which is why this heuristic still works. Anything with
  // two or fewer unicode codepoints is an extremely short word.
  if (supported_count == iter.char_offset() && iter.char_offset() <= 2)
    return true;

  return base::Contains(stopword_set_, base::UTF16ToUTF8(word_utf16));
}

}  // namespace content
