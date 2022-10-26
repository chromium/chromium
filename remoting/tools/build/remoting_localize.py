#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
localize.py -- Generates an output file from the given template replacing
variables and localizing strings.

The script uses Jinja2 template processing library (src/third_party/jinja2).
Variables available to the templates:
  - |languages| - the list of languages passed on the command line. ('-l').
  - Each NAME=VALUE define ('-d') can be accesses as {{ NAME }}.
  - |official_build| is set to '1' when CHROME_BUILD_TYPE environment variable
    is set to "_official".

Filters:
  - GetCodepage - returns the code page for the given language.
  - GetCodepageDecimal same as GetCodepage, but returns a decimal value.
  - GetLangId - returns Win32 LANGID.
  - GetPrimaryLanguage - returns a named Win32 constant specifing the primary
    language ID.
  - GetSublanguage -  returns a named Win32 constant specifing the sublanguage
    ID.

Globals:
  - IsRtlLanguage(language) - returns True if the language is right-to-left.
  - SelectLanguage(language) - allows to select the language to the used by
    {% trans %}{% endtrans %} statements.

"""

import io
import json
from optparse import OptionParser
import os
import sys
from string import Template


# Win32 primary languages IDs.
_LANGUAGE_PRIMARY = {
  'LANG_NEUTRAL' : 0x00,
  'LANG_INVARIANT' : 0x7f,
  'LANG_AFRIKAANS' : 0x36,
  'LANG_ALBANIAN' : 0x1c,
  'LANG_ALSATIAN' : 0x84,
  'LANG_AMHARIC' : 0x5e,
  'LANG_ARABIC' : 0x01,
  'LANG_ARMENIAN' : 0x2b,
  'LANG_ASSAMESE' : 0x4d,
  'LANG_AZERI' : 0x2c,
  'LANG_BASHKIR' : 0x6d,
  'LANG_BASQUE' : 0x2d,
  'LANG_BELARUSIAN' : 0x23,
  'LANG_BENGALI' : 0x45,
  'LANG_BRETON' : 0x7e,
  'LANG_BOSNIAN' : 0x1a,
  'LANG_BULGARIAN' : 0x02,
  'LANG_CATALAN' : 0x03,
  'LANG_CHINESE' : 0x04,
  'LANG_CORSICAN' : 0x83,
  'LANG_CROATIAN' : 0x1a,
  'LANG_CZECH' : 0x05,
  'LANG_DANISH' : 0x06,
  'LANG_DARI' : 0x8c,
  'LANG_DIVEHI' : 0x65,
  'LANG_DUTCH' : 0x13,
  'LANG_ENGLISH' : 0x09,
  'LANG_ESTONIAN' : 0x25,
  'LANG_FAEROESE' : 0x38,
  'LANG_FILIPINO' : 0x64,
  'LANG_FINNISH' : 0x0b,
  'LANG_FRENCH' : 0x0c,
  'LANG_FRISIAN' : 0x62,
  'LANG_GALICIAN' : 0x56,
  'LANG_GEORGIAN' : 0x37,
  'LANG_GERMAN' : 0x07,
  'LANG_GREEK' : 0x08,
  'LANG_GREENLANDIC' : 0x6f,
  'LANG_GUJARATI' : 0x47,
  'LANG_HAUSA' : 0x68,
  'LANG_HEBREW' : 0x0d,
  'LANG_HINDI' : 0x39,
  'LANG_HUNGARIAN' : 0x0e,
  'LANG_ICELANDIC' : 0x0f,
  'LANG_IGBO' : 0x70,
  'LANG_INDONESIAN' : 0x21,
  'LANG_INUKTITUT' : 0x5d,
  'LANG_IRISH' : 0x3c,
  'LANG_ITALIAN' : 0x10,
  'LANG_JAPANESE' : 0x11,
  'LANG_KANNADA' : 0x4b,
  'LANG_KASHMIRI' : 0x60,
  'LANG_KAZAK' : 0x3f,
  'LANG_KHMER' : 0x53,
  'LANG_KICHE' : 0x86,
  'LANG_KINYARWANDA' : 0x87,
  'LANG_KONKANI' : 0x57,
  'LANG_KOREAN' : 0x12,
  'LANG_KYRGYZ' : 0x40,
  'LANG_LAO' : 0x54,
  'LANG_LATVIAN' : 0x26,
  'LANG_LITHUANIAN' : 0x27,
  'LANG_LOWER_SORBIAN' : 0x2e,
  'LANG_LUXEMBOURGISH' : 0x6e,
  'LANG_MACEDONIAN' : 0x2f,
  'LANG_MALAY' : 0x3e,
  'LANG_MALAYALAM' : 0x4c,
  'LANG_MALTESE' : 0x3a,
  'LANG_MANIPURI' : 0x58,
  'LANG_MAORI' : 0x81,
  'LANG_MAPUDUNGUN' : 0x7a,
  'LANG_MARATHI' : 0x4e,
  'LANG_MOHAWK' : 0x7c,
  'LANG_MONGOLIAN' : 0x50,
  'LANG_NEPALI' : 0x61,
  'LANG_NORWEGIAN' : 0x14,
  'LANG_OCCITAN' : 0x82,
  'LANG_ORIYA' : 0x48,
  'LANG_PASHTO' : 0x63,
  'LANG_PERSIAN' : 0x29,
  'LANG_POLISH' : 0x15,
  'LANG_PORTUGUESE' : 0x16,
  'LANG_PUNJABI' : 0x46,
  'LANG_QUECHUA' : 0x6b,
  'LANG_ROMANIAN' : 0x18,
  'LANG_ROMANSH' : 0x17,
  'LANG_RUSSIAN' : 0x19,
  'LANG_SAMI' : 0x3b,
  'LANG_SANSKRIT' : 0x4f,
  'LANG_SCOTTISH_GAELIC' : 0x91,
  'LANG_SERBIAN' : 0x1a,
  'LANG_SINDHI' : 0x59,
  'LANG_SINHALESE' : 0x5b,
  'LANG_SLOVAK' : 0x1b,
  'LANG_SLOVENIAN' : 0x24,
  'LANG_SOTHO' : 0x6c,
  'LANG_SPANISH' : 0x0a,
  'LANG_SWAHILI' : 0x41,
  'LANG_SWEDISH' : 0x1d,
  'LANG_SYRIAC' : 0x5a,
  'LANG_TAJIK' : 0x28,
  'LANG_TAMAZIGHT' : 0x5f,
  'LANG_TAMIL' : 0x49,
  'LANG_TATAR' : 0x44,
  'LANG_TELUGU' : 0x4a,
  'LANG_THAI' : 0x1e,
  'LANG_TIBETAN' : 0x51,
  'LANG_TIGRIGNA' : 0x73,
  'LANG_TSWANA' : 0x32,
  'LANG_TURKISH' : 0x1f,
  'LANG_TURKMEN' : 0x42,
  'LANG_UIGHUR' : 0x80,
  'LANG_UKRAINIAN' : 0x22,
  'LANG_UPPER_SORBIAN' : 0x2e,
  'LANG_URDU' : 0x20,
  'LANG_UZBEK' : 0x43,
  'LANG_VIETNAMESE' : 0x2a,
  'LANG_WELSH' : 0x52,
  'LANG_WOLOF' : 0x88,
  'LANG_XHOSA' : 0x34,
  'LANG_YAKUT' : 0x85,
  'LANG_YI' : 0x78,
  'LANG_YORUBA' : 0x6a,
  'LANG_ZULU' : 0x35,
}


# Win32 sublanguage IDs.
_LANGUAGE_SUB = {
  'SUBLANG_NEUTRAL' : 0x00,
  'SUBLANG_DEFAULT' : 0x01,
  'SUBLANG_SYS_DEFAULT' : 0x02,
  'SUBLANG_CUSTOM_DEFAULT' : 0x03,
  'SUBLANG_CUSTOM_UNSPECIFIED' : 0x04,
  'SUBLANG_UI_CUSTOM_DEFAULT' : 0x05,
  'SUBLANG_AFRIKAANS_SOUTH_AFRICA' : 0x01,
  'SUBLANG_ALBANIAN_ALBANIA' : 0x01,
  'SUBLANG_ALSATIAN_FRANCE' : 0x01,
  'SUBLANG_AMHARIC_ETHIOPIA' : 0x01,
  'SUBLANG_ARABIC_SAUDI_ARABIA' : 0x01,
  'SUBLANG_ARABIC_IRAQ' : 0x02,
  'SUBLANG_ARABIC_EGYPT' : 0x03,
  'SUBLANG_ARABIC_LIBYA' : 0x04,
  'SUBLANG_ARABIC_ALGERIA' : 0x05,
  'SUBLANG_ARABIC_MOROCCO' : 0x06,
  'SUBLANG_ARABIC_TUNISIA' : 0x07,
  'SUBLANG_ARABIC_OMAN' : 0x08,
  'SUBLANG_ARABIC_YEMEN' : 0x09,
  'SUBLANG_ARABIC_SYRIA' : 0x0a,
  'SUBLANG_ARABIC_JORDAN' : 0x0b,
  'SUBLANG_ARABIC_LEBANON' : 0x0c,
  'SUBLANG_ARABIC_KUWAIT' : 0x0d,
  'SUBLANG_ARABIC_UAE' : 0x0e,
  'SUBLANG_ARABIC_BAHRAIN' : 0x0f,
  'SUBLANG_ARABIC_QATAR' : 0x10,
  'SUBLANG_ARMENIAN_ARMENIA' : 0x01,
  'SUBLANG_ASSAMESE_INDIA' : 0x01,
  'SUBLANG_AZERI_LATIN' : 0x01,
  'SUBLANG_AZERI_CYRILLIC' : 0x02,
  'SUBLANG_BASHKIR_RUSSIA' : 0x01,
  'SUBLANG_BASQUE_BASQUE' : 0x01,
  'SUBLANG_BELARUSIAN_BELARUS' : 0x01,
  'SUBLANG_BENGALI_INDIA' : 0x01,
  'SUBLANG_BENGALI_BANGLADESH' : 0x02,
  'SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN' : 0x05,
  'SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC' : 0x08,
  'SUBLANG_BRETON_FRANCE' : 0x01,
  'SUBLANG_BULGARIAN_BULGARIA' : 0x01,
  'SUBLANG_CATALAN_CATALAN' : 0x01,
  'SUBLANG_CHINESE_TRADITIONAL' : 0x01,
  'SUBLANG_CHINESE_SIMPLIFIED' : 0x02,
  'SUBLANG_CHINESE_HONGKONG' : 0x03,
  'SUBLANG_CHINESE_SINGAPORE' : 0x04,
  'SUBLANG_CHINESE_MACAU' : 0x05,
  'SUBLANG_CORSICAN_FRANCE' : 0x01,
  'SUBLANG_CZECH_CZECH_REPUBLIC' : 0x01,
  'SUBLANG_CROATIAN_CROATIA' : 0x01,
  'SUBLANG_CROATIAN_BOSNIA_HERZEGOVINA_LATIN' : 0x04,
  'SUBLANG_DANISH_DENMARK' : 0x01,
  'SUBLANG_DARI_AFGHANISTAN' : 0x01,
  'SUBLANG_DIVEHI_MALDIVES' : 0x01,
  'SUBLANG_DUTCH' : 0x01,
  'SUBLANG_DUTCH_BELGIAN' : 0x02,
  'SUBLANG_ENGLISH_US' : 0x01,
  'SUBLANG_ENGLISH_UK' : 0x02,
  'SUBLANG_ENGLISH_AUS' : 0x03,
  'SUBLANG_ENGLISH_CAN' : 0x04,
  'SUBLANG_ENGLISH_NZ' : 0x05,
  'SUBLANG_ENGLISH_EIRE' : 0x06,
  'SUBLANG_ENGLISH_SOUTH_AFRICA' : 0x07,
  'SUBLANG_ENGLISH_JAMAICA' : 0x08,
  'SUBLANG_ENGLISH_CARIBBEAN' : 0x09,
  'SUBLANG_ENGLISH_BELIZE' : 0x0a,
  'SUBLANG_ENGLISH_TRINIDAD' : 0x0b,
  'SUBLANG_ENGLISH_ZIMBABWE' : 0x0c,
  'SUBLANG_ENGLISH_PHILIPPINES' : 0x0d,
  'SUBLANG_ENGLISH_INDIA' : 0x10,
  'SUBLANG_ENGLISH_MALAYSIA' : 0x11,
  'SUBLANG_ENGLISH_SINGAPORE' : 0x12,
  'SUBLANG_ESTONIAN_ESTONIA' : 0x01,
  'SUBLANG_FAEROESE_FAROE_ISLANDS' : 0x01,
  'SUBLANG_FILIPINO_PHILIPPINES' : 0x01,
  'SUBLANG_FINNISH_FINLAND' : 0x01,
  'SUBLANG_FRENCH' : 0x01,
  'SUBLANG_FRENCH_BELGIAN' : 0x02,
  'SUBLANG_FRENCH_CANADIAN' : 0x03,
  'SUBLANG_FRENCH_SWISS' : 0x04,
  'SUBLANG_FRENCH_LUXEMBOURG' : 0x05,
  'SUBLANG_FRENCH_MONACO' : 0x06,
  'SUBLANG_FRISIAN_NETHERLANDS' : 0x01,
  'SUBLANG_GALICIAN_GALICIAN' : 0x01,
  'SUBLANG_GEORGIAN_GEORGIA' : 0x01,
  'SUBLANG_GERMAN' : 0x01,
  'SUBLANG_GERMAN_SWISS' : 0x02,
  'SUBLANG_GERMAN_AUSTRIAN' : 0x03,
  'SUBLANG_GERMAN_LUXEMBOURG' : 0x04,
  'SUBLANG_GERMAN_LIECHTENSTEIN' : 0x05,
  'SUBLANG_GREEK_GREECE' : 0x01,
  'SUBLANG_GREENLANDIC_GREENLAND' : 0x01,
  'SUBLANG_GUJARATI_INDIA' : 0x01,
  'SUBLANG_HAUSA_NIGERIA_LATIN' : 0x01,
  'SUBLANG_HEBREW_ISRAEL' : 0x01,
  'SUBLANG_HINDI_INDIA' : 0x01,
  'SUBLANG_HUNGARIAN_HUNGARY' : 0x01,
  'SUBLANG_ICELANDIC_ICELAND' : 0x01,
  'SUBLANG_IGBO_NIGERIA' : 0x01,
  'SUBLANG_INDONESIAN_INDONESIA' : 0x01,
  'SUBLANG_INUKTITUT_CANADA' : 0x01,
  'SUBLANG_INUKTITUT_CANADA_LATIN' : 0x02,
  'SUBLANG_IRISH_IRELAND' : 0x02,
  'SUBLANG_ITALIAN' : 0x01,
  'SUBLANG_ITALIAN_SWISS' : 0x02,
  'SUBLANG_JAPANESE_JAPAN' : 0x01,
  'SUBLANG_KANNADA_INDIA' : 0x01,
  'SUBLANG_KASHMIRI_SASIA' : 0x02,
  'SUBLANG_KASHMIRI_INDIA' : 0x02,
  'SUBLANG_KAZAK_KAZAKHSTAN' : 0x01,
  'SUBLANG_KHMER_CAMBODIA' : 0x01,
  'SUBLANG_KICHE_GUATEMALA' : 0x01,
  'SUBLANG_KINYARWANDA_RWANDA' : 0x01,
  'SUBLANG_KONKANI_INDIA' : 0x01,
  'SUBLANG_KOREAN' : 0x01,
  'SUBLANG_KYRGYZ_KYRGYZSTAN' : 0x01,
  'SUBLANG_LAO_LAO' : 0x01,
  'SUBLANG_LATVIAN_LATVIA' : 0x01,
  'SUBLANG_LITHUANIAN' : 0x01,
  'SUBLANG_LOWER_SORBIAN_GERMANY' : 0x02,
  'SUBLANG_LUXEMBOURGISH_LUXEMBOURG' : 0x01,
  'SUBLANG_MACEDONIAN_MACEDONIA' : 0x01,
  'SUBLANG_MALAY_MALAYSIA' : 0x01,
  'SUBLANG_MALAY_BRUNEI_DARUSSALAM' : 0x02,
  'SUBLANG_MALAYALAM_INDIA' : 0x01,
  'SUBLANG_MALTESE_MALTA' : 0x01,
  'SUBLANG_MAORI_NEW_ZEALAND' : 0x01,
  'SUBLANG_MAPUDUNGUN_CHILE' : 0x01,
  'SUBLANG_MARATHI_INDIA' : 0x01,
  'SUBLANG_MOHAWK_MOHAWK' : 0x01,
  'SUBLANG_MONGOLIAN_CYRILLIC_MONGOLIA' : 0x01,
  'SUBLANG_MONGOLIAN_PRC' : 0x02,
  'SUBLANG_NEPALI_INDIA' : 0x02,
  'SUBLANG_NEPALI_NEPAL' : 0x01,
  'SUBLANG_NORWEGIAN_BOKMAL' : 0x01,
  'SUBLANG_NORWEGIAN_NYNORSK' : 0x02,
  'SUBLANG_OCCITAN_FRANCE' : 0x01,
  'SUBLANG_ORIYA_INDIA' : 0x01,
  'SUBLANG_PASHTO_AFGHANISTAN' : 0x01,
  'SUBLANG_PERSIAN_IRAN' : 0x01,
  'SUBLANG_POLISH_POLAND' : 0x01,
  'SUBLANG_PORTUGUESE' : 0x02,
  'SUBLANG_PORTUGUESE_BRAZILIAN' : 0x01,
  'SUBLANG_PUNJABI_INDIA' : 0x01,
  'SUBLANG_QUECHUA_BOLIVIA' : 0x01,
  'SUBLANG_QUECHUA_ECUADOR' : 0x02,
  'SUBLANG_QUECHUA_PERU' : 0x03,
  'SUBLANG_ROMANIAN_ROMANIA' : 0x01,
  'SUBLANG_ROMANSH_SWITZERLAND' : 0x01,
  'SUBLANG_RUSSIAN_RUSSIA' : 0x01,
  'SUBLANG_SAMI_NORTHERN_NORWAY' : 0x01,
  'SUBLANG_SAMI_NORTHERN_SWEDEN' : 0x02,
  'SUBLANG_SAMI_NORTHERN_FINLAND' : 0x03,
  'SUBLANG_SAMI_LULE_NORWAY' : 0x04,
  'SUBLANG_SAMI_LULE_SWEDEN' : 0x05,
  'SUBLANG_SAMI_SOUTHERN_NORWAY' : 0x06,
  'SUBLANG_SAMI_SOUTHERN_SWEDEN' : 0x07,
  'SUBLANG_SAMI_SKOLT_FINLAND' : 0x08,
  'SUBLANG_SAMI_INARI_FINLAND' : 0x09,
  'SUBLANG_SANSKRIT_INDIA' : 0x01,
  'SUBLANG_SCOTTISH_GAELIC' : 0x01,
  'SUBLANG_SERBIAN_BOSNIA_HERZEGOVINA_LATIN' : 0x06,
  'SUBLANG_SERBIAN_BOSNIA_HERZEGOVINA_CYRILLIC' : 0x07,
  'SUBLANG_SERBIAN_MONTENEGRO_LATIN' : 0x0b,
  'SUBLANG_SERBIAN_MONTENEGRO_CYRILLIC' : 0x0c,
  'SUBLANG_SERBIAN_SERBIA_LATIN' : 0x09,
  'SUBLANG_SERBIAN_SERBIA_CYRILLIC' : 0x0a,
  'SUBLANG_SERBIAN_CROATIA' : 0x01,
  'SUBLANG_SERBIAN_LATIN' : 0x02,
  'SUBLANG_SERBIAN_CYRILLIC' : 0x03,
  'SUBLANG_SINDHI_INDIA' : 0x01,
  'SUBLANG_SINDHI_PAKISTAN' : 0x02,
  'SUBLANG_SINDHI_AFGHANISTAN' : 0x02,
  'SUBLANG_SINHALESE_SRI_LANKA' : 0x01,
  'SUBLANG_SOTHO_NORTHERN_SOUTH_AFRICA' : 0x01,
  'SUBLANG_SLOVAK_SLOVAKIA' : 0x01,
  'SUBLANG_SLOVENIAN_SLOVENIA' : 0x01,
  'SUBLANG_SPANISH' : 0x01,
  'SUBLANG_SPANISH_MEXICAN' : 0x02,
  'SUBLANG_SPANISH_MODERN' : 0x03,
  'SUBLANG_SPANISH_GUATEMALA' : 0x04,
  'SUBLANG_SPANISH_COSTA_RICA' : 0x05,
  'SUBLANG_SPANISH_PANAMA' : 0x06,
  'SUBLANG_SPANISH_DOMINICAN_REPUBLIC' : 0x07,
  'SUBLANG_SPANISH_VENEZUELA' : 0x08,
  'SUBLANG_SPANISH_COLOMBIA' : 0x09,
  'SUBLANG_SPANISH_PERU' : 0x0a,
  'SUBLANG_SPANISH_ARGENTINA' : 0x0b,
  'SUBLANG_SPANISH_ECUADOR' : 0x0c,
  'SUBLANG_SPANISH_CHILE' : 0x0d,
  'SUBLANG_SPANISH_URUGUAY' : 0x0e,
  'SUBLANG_SPANISH_PARAGUAY' : 0x0f,
  'SUBLANG_SPANISH_BOLIVIA' : 0x10,
  'SUBLANG_SPANISH_EL_SALVADOR' : 0x11,
  'SUBLANG_SPANISH_HONDURAS' : 0x12,
  'SUBLANG_SPANISH_NICARAGUA' : 0x13,
  'SUBLANG_SPANISH_PUERTO_RICO' : 0x14,
  'SUBLANG_SPANISH_US' : 0x15,
  'SUBLANG_SWAHILI_KENYA' : 0x01,
  'SUBLANG_SWEDISH' : 0x01,
  'SUBLANG_SWEDISH_FINLAND' : 0x02,
  'SUBLANG_SYRIAC_SYRIA' : 0x01,
  'SUBLANG_TAJIK_TAJIKISTAN' : 0x01,
  'SUBLANG_TAMAZIGHT_ALGERIA_LATIN' : 0x02,
  'SUBLANG_TAMIL_INDIA' : 0x01,
  'SUBLANG_TATAR_RUSSIA' : 0x01,
  'SUBLANG_TELUGU_INDIA' : 0x01,
  'SUBLANG_THAI_THAILAND' : 0x01,
  'SUBLANG_TIBETAN_PRC' : 0x01,
  'SUBLANG_TIGRIGNA_ERITREA' : 0x02,
  'SUBLANG_TSWANA_SOUTH_AFRICA' : 0x01,
  'SUBLANG_TURKISH_TURKEY' : 0x01,
  'SUBLANG_TURKMEN_TURKMENISTAN' : 0x01,
  'SUBLANG_UIGHUR_PRC' : 0x01,
  'SUBLANG_UKRAINIAN_UKRAINE' : 0x01,
  'SUBLANG_UPPER_SORBIAN_GERMANY' : 0x01,
  'SUBLANG_URDU_PAKISTAN' : 0x01,
  'SUBLANG_URDU_INDIA' : 0x02,
  'SUBLANG_UZBEK_LATIN' : 0x01,
  'SUBLANG_UZBEK_CYRILLIC' : 0x02,
  'SUBLANG_VIETNAMESE_VIETNAM' : 0x01,
  'SUBLANG_WELSH_UNITED_KINGDOM' : 0x01,
  'SUBLANG_WOLOF_SENEGAL' : 0x01,
  'SUBLANG_XHOSA_SOUTH_AFRICA' : 0x01,
  'SUBLANG_YAKUT_RUSSIA' : 0x01,
  'SUBLANG_YI_PRC' : 0x01,
  'SUBLANG_YORUBA_NIGERIA' : 0x01,
  'SUBLANG_ZULU_SOUTH_AFRICA' : 0x01,
}


'''
This dictionary defines the language lookup table. The key is the language ISO
country code, and the value specifies the corresponding code page, primary
language and sublanguage.

LCID resource: http://msdn.microsoft.com/en-us/library/ms776294.aspx
Codepage resource: http://www.science.co.il/language/locale-codes.asp
Language ID resource: http://msdn.microsoft.com/en-us/library/ms776294.aspx

There is no appropriate sublang for Spanish (Latin America) [es-419], so we
use Mexico. SUBLANG_DEFAULT would incorrectly map to Spain. Unlike other
Latin American countries, Mexican Spanish is supported by VERSIONINFO:
http://msdn.microsoft.com/en-us/library/aa381058.aspx

'''
_LANGUAGE_MAP = {
  # Language neutral LCID, unicode(1200) code page.
  'neutral' : [ 1200, 'LANG_NEUTRAL', 'SUBLANG_NEUTRAL' ],
  # LANG_USER_DEFAULT LCID, unicode(1200) code page.
  'userdefault' : [ 1200, 'LANG_NEUTRAL', 'SUBLANG_DEFAULT' ],
  'af' : [ 1252, 'LANG_AFRIKAANS', 'SUBLANG_DEFAULT' ],
  'am' : [ 1200, 'LANG_AMHARIC', 'SUBLANG_DEFAULT' ],
  'ar' : [ 1256, 'LANG_ARABIC', 'SUBLANG_DEFAULT' ],
  'bg' : [ 1251, 'LANG_BULGARIAN', 'SUBLANG_DEFAULT' ],
  'bn' : [ 1200, 'LANG_BENGALI', 'SUBLANG_DEFAULT' ],
  'ca' : [ 1252, 'LANG_CATALAN', 'SUBLANG_DEFAULT' ],
  'cs' : [ 1250, 'LANG_CZECH', 'SUBLANG_DEFAULT' ],
  'da' : [ 1252, 'LANG_DANISH', 'SUBLANG_DEFAULT' ],
  'de' : [ 1252, 'LANG_GERMAN', 'SUBLANG_GERMAN' ],
  'el' : [ 1253, 'LANG_GREEK', 'SUBLANG_DEFAULT' ],
  'en' : [ 1200, 'LANG_ENGLISH', 'SUBLANG_ENGLISH_US' ],
  'en-GB' : [ 1038, 'LANG_ENGLISH', 'SUBLANG_ENGLISH_UK' ],
  'es' : [ 1252, 'LANG_SPANISH', 'SUBLANG_SPANISH_MODERN' ],
  # LCID for Mexico; Windows does not support L.A. LCID.
  'es-419' : [ 1252, 'LANG_SPANISH', 'SUBLANG_SPANISH_MEXICAN' ],
  'et' : [ 1257, 'LANG_ESTONIAN', 'SUBLANG_DEFAULT' ],
  'eu' : [ 1252, 'LANG_BASQUE', 'SUBLANG_DEFAULT' ],
  'fa' : [ 1256, 'LANG_PERSIAN', 'SUBLANG_DEFAULT' ],
  'fi' : [ 1252, 'LANG_FINNISH', 'SUBLANG_DEFAULT' ],
  'fil' : [ 1252, 'LANG_FILIPINO', 'SUBLANG_DEFAULT' ],
  'fr' : [ 1252, 'LANG_FRENCH', 'SUBLANG_FRENCH' ],
  'fr-CA' : [ 1252, 'LANG_FRENCH', 'SUBLANG_FRENCH_CANADIAN' ],
  'gl' : [ 1252, 'LANG_GALICIAN', 'SUBLANG_DEFAULT' ],
  'gu' : [ 1200, 'LANG_GUJARATI', 'SUBLANG_DEFAULT' ],
  'he' : [ 1255, 'LANG_HEBREW', 'SUBLANG_DEFAULT' ],
  'hi' : [ 1200, 'LANG_HINDI', 'SUBLANG_DEFAULT' ],
  'hr' : [ 1252, 'LANG_CROATIAN', 'SUBLANG_DEFAULT' ],
  'hu' : [ 1250, 'LANG_HUNGARIAN', 'SUBLANG_DEFAULT' ],
  'id' : [ 1252, 'LANG_INDONESIAN', 'SUBLANG_DEFAULT' ],
  'is' : [ 1252, 'LANG_ICELANDIC', 'SUBLANG_DEFAULT' ],
  'it' : [ 1252, 'LANG_ITALIAN', 'SUBLANG_DEFAULT' ],
  'iw' : [ 1255, 'LANG_HEBREW', 'SUBLANG_DEFAULT' ],
  'ja' : [ 932, 'LANG_JAPANESE', 'SUBLANG_DEFAULT' ],
  'kn' : [ 1200, 'LANG_KANNADA', 'SUBLANG_DEFAULT' ],
  'ko' : [ 949, 'LANG_KOREAN', 'SUBLANG_KOREAN' ],
  'lt' : [ 1257, 'LANG_LITHUANIAN', 'SUBLANG_LITHUANIAN' ],
  'lv' : [ 1257, 'LANG_LATVIAN', 'SUBLANG_DEFAULT' ],
  'ml' : [ 1200, 'LANG_MALAYALAM', 'SUBLANG_DEFAULT' ],
  'mr' : [ 1200, 'LANG_MARATHI', 'SUBLANG_DEFAULT' ],
  # Malay (Malaysia) [ms-MY]
  'ms' : [ 1252, 'LANG_MALAY', 'SUBLANG_DEFAULT' ],
  'nb' : [ 1252, 'LANG_NORWEGIAN', 'SUBLANG_NORWEGIAN_BOKMAL' ],
  'ne' : [ 1200, 'LANG_NEPALI', 'SUBLANG_NEPALI_NEPAL' ],
  'nl' : [ 1252, 'LANG_DUTCH', 'SUBLANG_DEFAULT' ],
  'nn' : [ 1252, 'LANG_NORWEGIAN', 'SUBLANG_NORWEGIAN_NYNORSK' ],
  'no' : [ 1252, 'LANG_NORWEGIAN', 'SUBLANG_DEFAULT' ],
  'or' : [ 1200, 'LANG_ORIYA', 'SUBLANG_DEFAULT' ],
  'pa' : [ 1200, 'LANG_PUNJABI', 'SUBLANG_PUNJABI_INDIA' ],
  'pl' : [ 1250, 'LANG_POLISH', 'SUBLANG_DEFAULT' ],
  'pt-BR' : [ 1252, 'LANG_PORTUGUESE', 'SUBLANG_DEFAULT' ],
  'pt-PT' : [ 1252, 'LANG_PORTUGUESE', 'SUBLANG_PORTUGUESE' ],
  'ro' : [ 1250, 'LANG_ROMANIAN', 'SUBLANG_DEFAULT' ],
  'ru' : [ 1251, 'LANG_RUSSIAN', 'SUBLANG_DEFAULT' ],
  'sa' : [ 1200, 'LANG_SANSKRIT', 'SUBLANG_SANSKRIT_INDIA' ],
  'si' : [ 1200, 'LANG_SINHALESE', 'SUBLANG_SINHALESE_SRI_LANKA' ],
  'sk' : [ 1250, 'LANG_SLOVAK', 'SUBLANG_DEFAULT' ],
  'sl' : [ 1250, 'LANG_SLOVENIAN', 'SUBLANG_DEFAULT' ],
  'sr' : [ 1250, 'LANG_SERBIAN', 'SUBLANG_SERBIAN_LATIN' ],
  'sv' : [ 1252, 'LANG_SWEDISH', 'SUBLANG_SWEDISH' ],
  'sw' : [ 1252, 'LANG_SWAHILI', 'SUBLANG_DEFAULT' ],
  'ta' : [ 1200, 'LANG_TAMIL', 'SUBLANG_DEFAULT' ],
  'te' : [ 1200, 'LANG_TELUGU', 'SUBLANG_DEFAULT' ],
  'th' : [ 874, 'LANG_THAI', 'SUBLANG_DEFAULT' ],
  'ti' : [ 1200, 'LANG_TIGRIGNA', 'SUBLANG_TIGRIGNA_ERITREA' ],
  'tr' : [ 1254, 'LANG_TURKISH', 'SUBLANG_DEFAULT' ],
  'uk' : [ 1251, 'LANG_UKRAINIAN', 'SUBLANG_DEFAULT' ],
  'ur' : [ 1200, 'LANG_URDU', 'SUBLANG_DEFAULT' ],
  'vi' : [ 1258, 'LANG_VIETNAMESE', 'SUBLANG_DEFAULT' ],
  'zh-CN' : [ 936, 'LANG_CHINESE', 'SUBLANG_CHINESE_SIMPLIFIED' ],
  'zh-HK' : [ 950, 'LANG_CHINESE', 'SUBLANG_CHINESE_HONGKONG' ],
  'zh-TW' : [ 950, 'LANG_CHINESE', 'SUBLANG_CHINESE_TRADITIONAL' ],
  'zu' : [ 1200, 'LANG_ZULU', 'SUBLANG_DEFAULT' ],
}


# Right-To-Left languages
_RTL_LANGUAGES = (
  'ar',  # Arabic
  'fa',  # Farsi
  'iw',  # Hebrew
  'ks',  # Kashmiri
  'ku',  # Kurdish
  'ps',  # Pashto
  'ur',  # Urdu
  'yi',  # Yiddish
)


def GetCodepage(language):
  """ Returns the codepage for the given |language|. """
  lang = _LANGUAGE_MAP[language]
  return "%04x" % lang[0]


def GetCodepageDecimal(language):
  """ Returns the codepage for the given |language| as a decimal value. """
  lang = _LANGUAGE_MAP[language]
  return "%d" % lang[0]


def GetLangId(language):
  """ Returns the language id for the given |language|. """
  lang = _LANGUAGE_MAP[language]
  return "%04x" % (_LANGUAGE_PRIMARY[lang[1]] | (_LANGUAGE_SUB[lang[2]] << 10))


def GetPrimaryLanguage(language):
  """ Returns the primary language ID for the given |language|. """
  lang = _LANGUAGE_MAP[language]
  return _LANGUAGE_PRIMARY[lang[1]]


def GetSublanguage(language):
  """ Returns the sublanguage ID for the given |language|. """
  lang = _LANGUAGE_MAP[language]
  return _LANGUAGE_SUB[lang[2]]


def IsRtlLanguage(language):
  return language in _RTL_LANGUAGES;


def NormalizeLanguageCode(language):
  lang = language.replace('_', '-', 1)
  if lang == 'en-US':
    lang = 'en'
  return lang


def GetDataPackageSuffix(language):
  lang = NormalizeLanguageCode(language)
  if lang == 'en':
    lang = 'en-US'
  return lang


def GetJsonSuffix(language):
  return language.replace('-', '_', 1)


def ReadValuesFromFile(values_dict, file_name):
  """
  Reads NAME=VALUE settings from the specified file.

  Everything to the left of the first '=' is the keyword,
  everything to the right is the value.  No stripping of
  white space, so beware.

  The file must exist, otherwise you get the Python exception from open().
  """
  for line in open(file_name, 'r').readlines():
    key, val = line.rstrip('\r\n').split('=', 1)
    values_dict[key] = val


def ReadMessagesFromFile(file_name):
  """
  Reads messages from a 'chrome_messages_json' file.

  The file must exist, otherwise you get the Python exception from open().
  """
  messages_file = io.open(file_name, encoding='utf-8-sig')
  messages = json.load(messages_file)
  messages_file.close()

  values = {}
  for key in messages.keys():
    values[key] = messages[key]['message'];
  return values


def WriteIfChanged(file_name, contents, encoding):
  """
  Writes the specified contents to the specified file_name
  iff the contents are different than the current contents.
  """
  try:
    target = io.open(file_name, 'r')
    old_contents = target.read()
  except EnvironmentError:
    pass
  except UnicodeDecodeError:
    target.close()
    os.unlink(file_name)
  else:
    if contents == old_contents:
      return
    target.close()
    os.unlink(file_name)
  io.open(file_name, 'w', encoding=encoding).write(contents)


class MessageMap:
  """ Provides a dictionary of localized messages for each language."""
  def __init__(self, languages, locale_dir):
    self.language = None
    self.message_map = {}

    # Populate the message map
    if locale_dir:
      for language in languages:
        file_name = os.path.join(locale_dir,
                                 GetJsonSuffix(language),
                                 'messages.json')
        self.message_map[language] = ReadMessagesFromFile(file_name)

  def GetText(self, message):
    """ Returns a localized message for the current language. """
    try:
      return self.message_map[self.language][message]
    except:
      return self.message_map['en'][message]

  def SelectLanguage(self, language):
    """ Selects the language to be used when retrieving localized messages. """
    self.language = language

  def MakeSelectLanguage(self):
    """ Returns a function that can be used to select the current language. """
    return lambda language: self.SelectLanguage(language)

  def MakeGetText(self):
    """ Returns a function that can be used to retrieve a localized message. """
    return lambda message: self.GetText(message)


# Use '@' as a delimiter for string templates instead of '$' to avoid unintended
# expansion when passing the string from GYP.
class GypTemplate(Template):
    delimiter = '@'


def Localize(source, locales, options):
  # Set the list of languages to use.
  languages = map(NormalizeLanguageCode, locales)
  # Remove duplicates.
  languages = sorted(set(languages))
  context = { 'languages' : languages }

  # Load the localized messages.
  message_map = MessageMap(languages, options.locale_dir)

  # Add OFFICIAL_BUILD variable the same way build/util/version.py
  # does.
  if os.environ.get('CHROME_BUILD_TYPE') == '_official':
    context['official_build'] = '1'
  else:
    context['official_build'] = '0'

  # Add all variables defined in the command line.
  if options.define:
    for define in options.define:
      context.update(dict([define.split('=', 1)]));

  # Read NAME=VALUE variables from file.
  if options.variables:
    for file_name in options.variables:
      ReadValuesFromFile(context, file_name)

  env = None
  template = None

  if source:
    # Load jinja2 library.
    if options.jinja2:
      jinja2_path = os.path.normpath(options.jinja2)
    else:
      jinja2_path = os.path.normpath(
          os.path.join(os.path.abspath(__file__),
                       '../../../../third_party/jinja2'))
    # Insert after main module and before system modules.
    sys.path.insert(1, os.path.dirname(jinja2_path))
    from jinja2 import Environment, FileSystemLoader

    # Create jinja2 environment.
    (template_path, template_name) = os.path.split(source)
    env = Environment(loader=FileSystemLoader(template_path),
                      extensions=['jinja2.ext.do', 'jinja2.ext.i18n'])

    # Register custom filters.
    env.filters['GetCodepage'] = GetCodepage
    env.filters['GetCodepageDecimal'] = GetCodepageDecimal
    env.filters['GetLangId'] = GetLangId
    env.filters['GetPrimaryLanguage'] = GetPrimaryLanguage
    env.filters['GetSublanguage'] = GetSublanguage

    # Register the message map with jinja2.i18n extension.
    env.globals['IsRtlLanguage'] = IsRtlLanguage
    env.globals['SelectLanguage'] = message_map.MakeSelectLanguage()
    env.install_gettext_callables(message_map.MakeGetText(),
                                  message_map.MakeGetText());

    template = env.get_template(template_name)

  # Generate a separate file per each locale if requested.
  outputs = []
  if options.locale_output:
    target = GypTemplate(options.locale_output)
    for lang in languages:
      context['languages'] = [ lang ]
      context['language'] = lang
      context['pak_suffix'] = GetDataPackageSuffix(lang)
      context['json_suffix'] = GetJsonSuffix(lang)
      message_map.SelectLanguage(lang)

      template_file_name = target.safe_substitute(context)
      outputs.append(template_file_name)
      if not options.print_only and not options.locales_listfile:
        WriteIfChanged(template_file_name, template.render(context),
                       options.encoding)
  else:
    outputs.append(options.output)
    if not options.print_only:
      WriteIfChanged(options.output, template.render(context), options.encoding)

  if options.print_only:
    # Quote each element so filename spaces don't mess up gyp's attempt to parse
    # it into a list.
    return " ".join(['"%s"' % x for x in outputs])

  if options.locales_listfile:
    # Strip off the quotes from each filename when writing into a listfile.
    content = u'\n'.join([x.strip('"') for x in outputs])
    WriteIfChanged(options.locales_listfile, content, options.encoding)

  return


def DoMain(argv):
  usage = "Usage: localize [options] locales"
  parser = OptionParser(usage=usage)
  parser.add_option(
      '-d', '--define', dest='define', action='append', type='string',
      help='define a variable (NAME=VALUE).')
  parser.add_option(
      '--encoding', dest='encoding', type='string', default='utf-8',
      help="set the encoding of <output>. 'utf-8' is the default.")
  parser.add_option(
      '--jinja2', dest='jinja2', type='string',
      help="specifies path to the jinja2 library.")
  parser.add_option(
      '--locale_dir', dest='locale_dir', type='string',
      help="set path to localized message files.")
  parser.add_option(
      '--locale_output', dest='locale_output',  type='string',
      help='specify the per-locale output file name.')
  parser.add_option(
      '-o', '--output', dest='output', type='string',
      help="specify the output file name.")
  parser.add_option(
      '--print_only', dest='print_only', action='store_true',
      default=False, help='print the output file names only.')
  parser.add_option(
      '--locales_listfile', dest='locales_listfile', type='string',
      help='print the output file names into the specified file.')
  parser.add_option(
      '-t', '--template', dest='template', type='string',
      help="specify the template file name.")
  parser.add_option(
      '--variables', dest='variables', action='append', type='string',
      help='read variables (NAME=VALUE) from file.')

  options, locales = parser.parse_args(argv)
  if not locales:
    parser.error('At least one locale must be specified')
  if bool(options.output) == bool(options.locale_output):
    parser.error(
        'Either --output or --locale_output must be specified but not both')
  if (not options.template and
      not options.print_only and not options.locales_listfile):
    parser.error('The template name is required unless either --print_only '
                 'or --locales_listfile is used')

  return Localize(options.template, locales, options)

if __name__ == '__main__':
  sys.exit(DoMain(sys.argv[1:]))
