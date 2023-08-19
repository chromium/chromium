// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/counter_style_map.h"

#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

bool IsPredefinedSymbolMarkerName(const AtomicString& name) {
  static const AtomicString* predefined_symbol_markers[] = {
      &keywords::kDisc, &keywords::kSquare, &keywords::kCircle,
      &keywords::kDisclosureOpen, &keywords::kDisclosureClosed};
  for (const AtomicString* predefined_name : predefined_symbol_markers) {
    if (*predefined_name == name) {
      return true;
    }
  }
  return false;
}

HashMap<AtomicString, String> CollectUACounterStyleRules() {
  HashMap<AtomicString, String> ua_rules;

  // https://drafts.csswg.org/css-counter-styles-3/#simple-numeric

  ua_rules.Set(keywords::kDecimal, R"CSS(
    system: numeric;
    symbols: '0' '1' '2' '3' '4' '5' '6' '7' '8' '9';
  )CSS");

  ua_rules.Set(AtomicString("decimal-leading-zero"), R"CSS(
    system: extends decimal;
    pad: 2 '0';
  )CSS");

  // ٠ ١ ٢ ٣ ٤ ٥ ٦ ٧ ٨ ٩
  ua_rules.Set(AtomicString("arabic-indic"), R"CSS(
    system: numeric;
    symbols: "\660" "\661" "\662" "\663" "\664" "\665" "\666" "\667" "\668" "\669";
  )CSS");

  // Officially, 'armenian' is specified as an additive counter style supporting
  // 1-9999. We extend the range to 99999999 using a special algorithm.
  ua_rules.Set(AtomicString("armenian"), R"CSS(
    system: -internal-upper-armenian;
    range: 1 99999999;
  )CSS");

  ua_rules.Set(AtomicString("upper-armenian"), R"CSS(
    system: extends armenian;
  )CSS");

  // Officially, 'lower-armenian' is specified as an additive counter style
  // supporting 1-9999. We extend the range to 99999999 using a special
  // algorithm.
  ua_rules.Set(AtomicString("lower-armenian"), R"CSS(
    system: -internal-lower-armenian;
    range: 1 99999999;
  )CSS");

  // ০ ১ ২ ৩ ৪ ৫ ৬ ৭ ৮ ৯
  ua_rules.Set(AtomicString("bengali"), R"CSS(
    system: numeric;
    symbols: "\9E6" "\9E7" "\9E8" "\9E9" "\9EA" "\9EB" "\9EC" "\9ED" "\9EE" "\9EF";
  )CSS");

  // ០ ១ ២ ៣ ៤ ៥ ៦ ៧ ៨ ៩
  ua_rules.Set(AtomicString("cambodian"), R"CSS(
    system: numeric;
    symbols: "\17E0" "\17E1" "\17E2" "\17E3" "\17E4" "\17E5" "\17E6" "\17E7" "\17E8" "\17E9";
  )CSS");

  ua_rules.Set(AtomicString("khmer"), R"CSS(
    system: extends cambodian;
  )CSS");

  // symbols: 〇 一 二 三 四 五 六 七 八 九
  // suffix: "、"
  ua_rules.Set(AtomicString("cjk-decimal"), R"CSS(
    system: numeric;
    range: 0 infinite;
    symbols: "\3007" "\4E00" "\4E8C" "\4E09" "\56DB" "\4E94" "\516D" "\4E03" "\516B" "\4E5D";
    suffix: "\3001";
  )CSS");

  // ० १ २ ३ ४ ५ ६ ७ ८ ९
  ua_rules.Set(AtomicString("devanagari"), R"CSS(
    system: numeric;
    symbols: "\966" "\967" "\968" "\969" "\96A" "\96B" "\96C" "\96D" "\96E" "\96F";
  )CSS");

  // 10000 ჵ, 9000 ჰ, 8000 ჯ, 7000 ჴ, 6000 ხ, 5000 ჭ, 4000 წ,
  // 3000 ძ, 2000 ც, 1000 ჩ, 900 შ, 800 ყ, 700 ღ, 600 ქ, 500 ფ, 400 ჳ, 300 ტ,
  // 200 ს, 100 რ, 90 ჟ, 80 პ, 70 ო, 60 ჲ, 50 ნ, 40 მ, 30 ლ, 20 კ, 10 ი, 9 თ, 8
  // ჱ, 7 ზ, 6 ვ, 5 ე, 4 დ, 3 გ, 2 ბ, 1 ა
  ua_rules.Set(AtomicString("georgian"), R"CSS(
    system: additive;
    range: 1 19999;
    additive-symbols: 10000 \10F5, 9000 \10F0, 8000 \10EF, 7000 \10F4, 6000 \10EE, 5000 \10ED, 4000 \10EC, 3000 \10EB, 2000 \10EA, 1000 \10E9, 900 \10E8, 800 \10E7, 700 \10E6, 600 \10E5, 500 \10E4, 400 \10F3, 300 \10E2, 200 \10E1, 100 \10E0, 90 \10DF, 80 \10DE, 70 \10DD, 60 \10F2, 50 \10DC, 40 \10DB, 30 \10DA, 20 \10D9, 10 \10D8, 9 \10D7, 8 \10F1, 7 \10D6, 6 \10D5, 5 \10D4, 4 \10D3, 3 \10D2, 2 \10D1, 1 \10D0;
  )CSS");

  // ૦ ૧ ૨ ૩ ૪ ૫ ૬ ૭ ૮ ૯
  ua_rules.Set(AtomicString("gujarati"), R"CSS(
    system: numeric;
    symbols: "\AE6" "\AE7" "\AE8" "\AE9" "\AEA" "\AEB" "\AEC" "\AED" "\AEE" "\AEF";
  )CSS");

  // ੦ ੧ ੨ ੩ ੪ ੫ ੬ ੭ ੮ ੯
  ua_rules.Set(AtomicString("gurmukhi"), R"CSS(
    system: numeric;
    symbols: "\A66" "\A67" "\A68" "\A69" "\A6A" "\A6B" "\A6C" "\A6D" "\A6E" "\A6F";
  )CSS");

  // Officially, 'hebrew' is specified as an additive counter style supporting
  // 1-10999. We extend the range to 0-999999 using a special algorithm.
  ua_rules.Set(AtomicString("hebrew"), R"CSS(
    system: -internal-hebrew;
    range: 0 999999;
  )CSS");

  // ೦ ೧ ೨ ೩ ೪ ೫ ೬ ೭ ೮ ೯
  ua_rules.Set(AtomicString("kannada"), R"CSS(
    system: numeric;
    symbols: "\CE6" "\CE7" "\CE8" "\CE9" "\CEA" "\CEB" "\CEC" "\CED" "\CEE" "\CEF";
  )CSS");

  // ໐ ໑ ໒ ໓ ໔ ໕ ໖ ໗ ໘ ໙
  ua_rules.Set(AtomicString("lao"), R"CSS(
    system: numeric;
    symbols: "\ED0" "\ED1" "\ED2" "\ED3" "\ED4" "\ED5" "\ED6" "\ED7" "\ED8" "\ED9";
  )CSS");

  // ൦ ൧ ൨ ൩ ൪ ൫ ൬ ൭ ൮ ൯
  ua_rules.Set(AtomicString("malayalam"), R"CSS(
    system: numeric;
    symbols: "\D66" "\D67" "\D68" "\D69" "\D6A" "\D6B" "\D6C" "\D6D" "\D6E" "\D6F";
  )CSS");

  // ᠐ ᠑ ᠒ ᠓ ᠔ ᠕ ᠖ ᠗ ᠘ ᠙
  ua_rules.Set(AtomicString("mongolian"), R"CSS(
    system: numeric;
    symbols: "\1810" "\1811" "\1812" "\1813" "\1814" "\1815" "\1816" "\1817" "\1818" "\1819";
  )CSS");

  // ၀ ၁ ၂ ၃ ၄ ၅ ၆ ၇ ၈ ၉
  ua_rules.Set(AtomicString("myanmar"), R"CSS(
    system: numeric;
    symbols: "\1040" "\1041" "\1042" "\1043" "\1044" "\1045" "\1046" "\1047" "\1048" "\1049";
  )CSS");

  // ୦ ୧ ୨ ୩ ୪ ୫ ୬ ୭ ୮ ୯
  ua_rules.Set(AtomicString("oriya"), R"CSS(
    system: numeric;
    symbols: "\B66" "\B67" "\B68" "\B69" "\B6A" "\B6B" "\B6C" "\B6D" "\B6E" "\B6F";
  )CSS");

  // ۰ ۱ ۲ ۳ ۴ ۵ ۶ ۷ ۸ ۹
  ua_rules.Set(AtomicString("persian"), R"CSS(
    system: numeric;
    symbols: "\6F0" "\6F1" "\6F2" "\6F3" "\6F4" "\6F5" "\6F6" "\6F7" "\6F8" "\6F9";
  )CSS");

  ua_rules.Set(keywords::kLowerRoman, R"CSS(
    system: additive;
    range: 1 3999;
    additive-symbols: 1000 m, 900 cm, 500 d, 400 cd, 100 c, 90 xc, 50 l, 40 xl, 10 x, 9 ix, 5 v, 4 iv, 1 i;
  )CSS");

  ua_rules.Set(keywords::kUpperRoman, R"CSS(
    system: additive;
    range: 1 3999;
    additive-symbols: 1000 M, 900 CM, 500 D, 400 CD, 100 C, 90 XC, 50 L, 40 XL, 10 X, 9 IX, 5 V, 4 IV, 1 I;
  )CSS");

  // ௦ ௧ ௨ ௩ ௪ ௫ ௬ ௭ ௮ ௯
  ua_rules.Set(AtomicString("tamil"), R"CSS(
    system: numeric;
    symbols: "\BE6" "\BE7" "\BE8" "\BE9" "\BEA" "\BEB" "\BEC" "\BED" "\BEE" "\BEF";
  )CSS");

  // ౦ ౧ ౨ ౩ ౪ ౫ ౬ ౭ ౮ ౯
  ua_rules.Set(AtomicString("telugu"), R"CSS(
    system: numeric;
    symbols: "\C66" "\C67" "\C68" "\C69" "\C6A" "\C6B" "\C6C" "\C6D" "\C6E" "\C6F";
  )CSS");

  // ๐ ๑ ๒ ๓ ๔ ๕ ๖ ๗ ๘ ๙
  ua_rules.Set(AtomicString("thai"), R"CSS(
    system: numeric;
    symbols: "\E50" "\E51" "\E52" "\E53" "\E54" "\E55" "\E56" "\E57" "\E58" "\E59";
  )CSS");

  // ༠ ༡ ༢ ༣ ༤ ༥ ༦ ༧ ༨ ༩
  ua_rules.Set(AtomicString("tibetan"), R"CSS(
    system: numeric;
    symbols: "\F20" "\F21" "\F22" "\F23" "\F24" "\F25" "\F26" "\F27" "\F28" "\F29";
  )CSS");

  // https://drafts.csswg.org/css-counter-styles-3/#simple-alphabetic

  ua_rules.Set(keywords::kLowerAlpha, R"CSS(
    system: alphabetic;
    symbols: a b c d e f g h i j k l m n o p q r s t u v w x y z;
  )CSS");

  ua_rules.Set(AtomicString("lower-latin"), R"CSS(
    system: extends lower-alpha;
  )CSS");

  ua_rules.Set(keywords::kUpperAlpha, R"CSS(
    system: alphabetic;
    symbols: A B C D E F G H I J K L M N O P Q R S T U V W X Y Z;
  )CSS");

  ua_rules.Set(AtomicString("upper-latin"), R"CSS(
    system: extends upper-alpha;
  )CSS");

  // α β γ δ ε ζ η θ ι κ λ μ ν ξ ο π ρ σ τ υ φ χ ψ ω
  ua_rules.Set(AtomicString("lower-greek"), R"CSS(
    system: alphabetic;
    symbols: "\3B1" "\3B2" "\3B3" "\3B4" "\3B5" "\3B6" "\3B7" "\3B8" "\3B9" "\3BA" "\3BB" "\3BC" "\3BD" "\3BE" "\3BF" "\3C0" "\3C1" "\3C3" "\3C4" "\3C5" "\3C6" "\3C7" "\3C8" "\3C9";
  )CSS");

  // あ い う え お か き く け こ さ し す せ そ た ち つ て と な に ぬ
  // ね の は ひ ふ へ ほ ま み む め も や ゆ よ ら り る れ ろ わ ゐ ゑ を ん
  ua_rules.Set(AtomicString("hiragana"), R"CSS(
    system: alphabetic;
    symbols: "\3042" "\3044" "\3046" "\3048" "\304A" "\304B" "\304D" "\304F" "\3051" "\3053" "\3055" "\3057" "\3059" "\305B" "\305D" "\305F" "\3061" "\3064" "\3066" "\3068" "\306A" "\306B" "\306C" "\306D" "\306E" "\306F" "\3072" "\3075" "\3078" "\307B" "\307E" "\307F" "\3080" "\3081" "\3082" "\3084" "\3086" "\3088" "\3089" "\308A" "\308B" "\308C" "\308D" "\308F" "\3090" "\3091" "\3092" "\3093";
    suffix: "\3001";
  )CSS");

  // い ろ は に ほ へ と ち り ぬ る を わ か よ た れ そ つ ね な ら む
  // う ゐ の お く や ま け ふ こ え て あ さ き ゆ め み し ゑ ひ も せ す
  ua_rules.Set(AtomicString("hiragana-iroha"), R"CSS(
    system: alphabetic;
    symbols: "\3044" "\308D" "\306F" "\306B" "\307B" "\3078" "\3068" "\3061" "\308A" "\306C" "\308B" "\3092" "\308F" "\304B" "\3088" "\305F" "\308C" "\305D" "\3064" "\306D" "\306A" "\3089" "\3080" "\3046" "\3090" "\306E" "\304A" "\304F" "\3084" "\307E" "\3051" "\3075" "\3053" "\3048" "\3066" "\3042" "\3055" "\304D" "\3086" "\3081" "\307F" "\3057" "\3091" "\3072" "\3082" "\305B" "\3059";
    suffix: "\3001";
  )CSS");

  // ア イ ウ エ オ カ キ ク ケ コ サ シ ス セ ソ タ チ ツ テ ト ナ ニ ヌ
  // ネ ノ ハ ヒ フ ヘ ホ マ ミ ム メ モ ヤ ユ ヨ ラ リ ル レ ロ ワ ヰ ヱ ヲ ン
  ua_rules.Set(AtomicString("katakana"), R"CSS(
    system: alphabetic;
    symbols: "\30A2" "\30A4" "\30A6" "\30A8" "\30AA" "\30AB" "\30AD" "\30AF" "\30B1" "\30B3" "\30B5" "\30B7" "\30B9" "\30BB" "\30BD" "\30BF" "\30C1" "\30C4" "\30C6" "\30C8" "\30CA" "\30CB" "\30CC" "\30CD" "\30CE" "\30CF" "\30D2" "\30D5" "\30D8" "\30DB" "\30DE" "\30DF" "\30E0" "\30E1" "\30E2" "\30E4" "\30E6" "\30E8" "\30E9" "\30EA" "\30EB" "\30EC" "\30ED" "\30EF" "\30F0" "\30F1" "\30F2" "\30F3";
    suffix: "\3001";
  )CSS");

  // イ ロ ハ ニ ホ ヘ ト チ リ ヌ ル ヲ ワ カ ヨ タ レ ソ ツ ネ ナ ラ ム
  // ウ ヰ ノ オ ク ヤ マ ケ フ コ エ テ ア サ キ ユ メ ミ シ ヱ ヒ モ セ ス */
  ua_rules.Set(AtomicString("katakana-iroha"), R"CSS(
    system: alphabetic;
    symbols: "\30A4" "\30ED" "\30CF" "\30CB" "\30DB" "\30D8" "\30C8" "\30C1" "\30EA" "\30CC" "\30EB" "\30F2" "\30EF" "\30AB" "\30E8" "\30BF" "\30EC" "\30BD" "\30C4" "\30CD" "\30CA" "\30E9" "\30E0" "\30A6" "\30F0" "\30CE" "\30AA" "\30AF" "\30E4" "\30DE" "\30B1" "\30D5" "\30B3" "\30A8" "\30C6" "\30A2" "\30B5" "\30AD" "\30E6" "\30E1" "\30DF" "\30B7" "\30F1" "\30D2" "\30E2" "\30BB" "\30B9";
    suffix: "\3001";
  )CSS");

  // https://drafts.csswg.org/css-counter-styles-3/#simple-symbolic

  // •
  ua_rules.Set(keywords::kDisc, R"CSS(
    system: cyclic;
    symbols: \2022;
    suffix: " ";
  )CSS");

  // ◦
  ua_rules.Set(keywords::kCircle, R"CSS(
    system: cyclic;
    symbols: \25E6;
    suffix: " ";
  )CSS");

  // Note: Spec requires \25FE, but we've always been using \25A0.
  ua_rules.Set(keywords::kSquare, R"CSS(
    system: cyclic;
    symbols: \25A0;
    suffix: " ";
  )CSS");

  ua_rules.Set(keywords::kDisclosureOpen, R"CSS(
    system: cyclic;
    symbols: \25BE;
    suffix: " ";
  )CSS");

  ua_rules.Set(keywords::kDisclosureClosed, R"CSS(
    system: cyclic;
    symbols: \25B8;
    suffix: " ";
  )CSS");

  // https://drafts.csswg.org/css-counter-styles-3/#simple-fixed

  // 子 丑 寅 卯 辰 巳 午 未 申 酉 戌 亥
  ua_rules.Set(AtomicString("cjk-earthly-branch"), R"CSS(
    system: fixed;
    symbols: "\5B50" "\4E11" "\5BC5" "\536F" "\8FB0" "\5DF3" "\5348" "\672A" "\7533" "\9149" "\620C" "\4EA5";
    suffix: "\3001";
    fallback: cjk-decimal;
  )CSS");

  // 甲 乙 丙 丁 戊 己 庚 辛 壬 癸
  ua_rules.Set(AtomicString("cjk-heavenly-stem"), R"CSS(
    system: fixed;
    symbols: "\7532" "\4E59" "\4E19" "\4E01" "\620A" "\5DF1" "\5E9A" "\8F9B" "\58EC" "\7678";
    suffix: "\3001";
    fallback: cjk-decimal;
  )CSS");

  // https://drafts.csswg.org/css-counter-styles-3/#limited-japanese

  // 9000 九千, 8000 八千, 7000 七千, 6000 六千, 5000 五千, 4000 四千, 3000
  // 三千, 2000 二千, 1000 千, 900 九百, 800 八百, 700 七百, 600 六百, 500 五百,
  // 400 四百, 300 三百, 200 二百, 100 百, 90 九十, 80 八十, 70 七十, 60 六十,
  // 50 五十, 40 四十, 30 三十, 20 二十, 10 十, 9 九, 8 八, 7 七, 6 六, 5 五, 4
  // 四, 3 三, 2 二, 1 一, 0 〇
  ua_rules.Set(AtomicString("japanese-informal"), R"CSS(
    system: additive;
    range: -9999 9999;
    additive-symbols: 9000 \4E5D\5343, 8000 \516B\5343, 7000 \4E03\5343, 6000 \516D\5343, 5000 \4E94\5343, 4000 \56DB\5343, 3000 \4E09\5343, 2000 \4E8C\5343, 1000 \5343, 900 \4E5D\767E, 800 \516B\767E, 700 \4E03\767E, 600 \516D\767E, 500 \4E94\767E, 400 \56DB\767E, 300 \4E09\767E, 200 \4E8C\767E, 100 \767E, 90 \4E5D\5341, 80 \516B\5341, 70 \4E03\5341, 60 \516D\5341, 50 \4E94\5341, 40 \56DB\5341, 30 \4E09\5341, 20 \4E8C\5341, 10 \5341, 9 \4E5D, 8 \516B, 7 \4E03, 6 \516D, 5 \4E94, 4 \56DB, 3 \4E09, 2 \4E8C, 1 \4E00, 0 \3007;
    suffix: '\3001';
    negative: "\30DE\30A4\30CA\30B9";
    fallback: cjk-decimal;
  )CSS");

  // 9000 九阡, 8000 八阡, 7000 七阡, 6000 六阡, 5000 伍阡, 4000 四阡, 3000
  // 参阡, 2000 弐阡, 1000 壱阡, 900 九百, 800 八百, 700 七百, 600 六百, 500
  // 伍百, 400 四百, 300 参百, 200 弐百, 100 壱百, 90 九拾, 80 八拾, 70 七拾, 60
  // 六拾, 50 伍拾, 40 四拾, 30 参拾, 20 弐拾, 10 壱拾, 9 九, 8 八, 7 七, 6 六,
  // 5 伍, 4 四, 3 参, 2 弐, 1 壱, 0 零
  ua_rules.Set(AtomicString("japanese-formal"), R"CSS(
    system: additive;
    range: -9999 9999;
    additive-symbols: 9000 \4E5D\9621, 8000 \516B\9621, 7000 \4E03\9621, 6000 \516D\9621, 5000 \4F0D\9621, 4000 \56DB\9621, 3000 \53C2\9621, 2000 \5F10\9621, 1000 \58F1\9621, 900 \4E5D\767E, 800 \516B\767E, 700 \4E03\767E, 600 \516D\767E, 500 \4F0D\767E, 400 \56DB\767E, 300 \53C2\767E, 200 \5F10\767E, 100 \58F1\767E, 90 \4E5D\62FE, 80 \516B\62FE, 70 \4E03\62FE, 60 \516D\62FE, 50 \4F0D\62FE, 40 \56DB\62FE, 30 \53C2\62FE, 20 \5F10\62FE, 10 \58F1\62FE, 9 \4E5D, 8 \516B, 7 \4E03, 6 \516D, 5 \4F0D, 4 \56DB, 3 \53C2, 2 \5F10, 1 \58F1, 0 \96F6;
    suffix: '\3001';
    negative: "\30DE\30A4\30CA\30B9";
    fallback: cjk-decimal;
  )CSS");

  // https://drafts.csswg.org/css-counter-styles-3/#limited-korean

  // Note: While the officially specified range is -9999 to 9999 for these
  // counter styles, implementations are allowed to support a larger range for
  // investigative purposes. Therefore, we support the full int range.

  // negative: 마이너스 (followed by a space)
  ua_rules.Set(AtomicString("korean-hangul-formal"), R"CSS(
    system: -internal-korean-hangul-formal;
    suffix: ', ';
    negative: "\B9C8\C774\B108\C2A4  ";
    fallback: cjk-decimal;
  )CSS");

  // negative: 마이너스 (followed by a space)
  ua_rules.Set(AtomicString("korean-hanja-informal"), R"CSS(
    system: -internal-korean-hanja-informal;
    suffix: ', ';
    negative: "\B9C8\C774\B108\C2A4  ";
    fallback: cjk-decimal;
  )CSS");

  // negative: 마이너스 (followed by a space)
  ua_rules.Set(AtomicString("korean-hanja-formal"), R"CSS(
    system: -internal-korean-hanja-formal;
    suffix: ', ';
    negative: "\B9C8\C774\B108\C2A4  ";
    fallback: cjk-decimal;
  )CSS");

  // https://drafts.csswg.org/css-counter-styles-3/#limited-chinese

  // Note: While the officially specified range is -9999 to 9999 for these
  // counter styles, implementations are allowed to support a larger range for
  // investigative purposes. Therefore, we support the full int range.

  // negative: 负
  ua_rules.Set(AtomicString("simp-chinese-informal"), R"CSS(
    system: -internal-simp-chinese-informal;
    suffix: \3001;
    negative: \8D1F;
    fallback: cjk-decimal;
  )CSS");

  // negative: 负
  ua_rules.Set(AtomicString("simp-chinese-formal"), R"CSS(
    system: -internal-simp-chinese-formal;
    suffix: \3001;
    negative: \8D1F;
    fallback: cjk-decimal;
  )CSS");

  // negative: 負
  ua_rules.Set(AtomicString("trad-chinese-informal"), R"CSS(
    system: -internal-trad-chinese-informal;
    suffix: \3001;
    negative: \8CA0;
    fallback: cjk-decimal;
  )CSS");

  // negative: 負
  ua_rules.Set(AtomicString("trad-chinese-formal"), R"CSS(
    system: -internal-trad-chinese-formal;
    suffix: \3001;
    negative: \8CA0;
    fallback: cjk-decimal;
  )CSS");

  ua_rules.Set(AtomicString("cjk-ideographic"), R"CSS(
    system: extends trad-chinese-informal;
  )CSS");

  // https://drafts.csswg.org/css-counter-styles-3/#ethiopic-numeric-counter-style

  ua_rules.Set(AtomicString("ethiopic-numeric"), R"CSS(
    system: -internal-ethiopic-numeric;
    range: 1 infinite;
    suffix: "/ ";
  )CSS");

  // Non-standard counter styles that we've been supporting

  ua_rules.Set(AtomicString("ethiopic-halehame"), R"CSS(
    system: alphabetic;
    symbols: '\1200' '\1208' '\1210' '\1218' '\1220' '\1228' '\1230' '\1240' '\1260' '\1270' '\1280' '\1290' '\12A0' '\12A8' '\12C8' '\12D0' '\12D8' '\12E8' '\12F0' '\1308' '\1320' '\1330' '\1338' '\1340' '\1348' '\1350';
    suffix: '\1366 ';
  )CSS");

  ua_rules.Set(AtomicString("ethiopic-halehame-am"), R"CSS(
    system: alphabetic;
    symbols: '\1200' '\1208' '\1210' '\1218' '\1220' '\1228' '\1230' '\1238' '\1240' '\1260' '\1270' '\1278' '\1280' '\1290' '\1298' '\12A0' '\12A8' '\12B8' '\12C8' '\12D0' '\12D8' '\12E0' '\12E8' '\12F0' '\1300' '\1308' '\1320' '\1328' '\1330' '\1338' '\1340' '\1348' '\1350';
    suffix: '\1366 ';
  )CSS");

  ua_rules.Set(AtomicString("ethiopic-halehame-ti-er"), R"CSS(
    system: alphabetic;
    symbols: '\1200' '\1208' '\1210' '\1218' '\1228' '\1230' '\1238' '\1240' '\1250' '\1260' '\1270' '\1278' '\1290' '\1298' '\12A0' '\12A8' '\12B8' '\12C8' '\12D0' '\12D8' '\12E0' '\12E8' '\12F0' '\1300' '\1308' '\1320' '\1328' '\1330' '\1338' '\1348' '\1350';
    suffix: '\1366 ';
  )CSS");

  ua_rules.Set(AtomicString("ethiopic-halehame-ti-et"), R"CSS(
    system: alphabetic;
    symbols: '\1200' '\1208' '\1210' '\1218' '\1220' '\1228' '\1230' '\1238' '\1240' '\1250' '\1260' '\1270' '\1278' '\1280' '\1290' '\1298' '\12A0' '\12A8' '\12B8' '\12C8' '\12D0' '\12D8' '\12E0' '\12E8' '\12F0' '\1300' '\1308' '\1320' '\1328' '\1330' '\1338' '\1340' '\1348' '\1350';
    suffix: '\1366 ';
  )CSS");

  ua_rules.Set(AtomicString("hangul"), R"CSS(
    system: alphabetic;
    symbols: '\AC00' '\B098' '\B2E4' '\B77C' '\B9C8' '\BC14' '\C0AC' '\C544' '\C790' '\CC28' '\CE74' '\D0C0' '\D30C' '\D558';
  )CSS");

  ua_rules.Set(AtomicString("hangul-consonant"), R"CSS(
    system: alphabetic;
    symbols: '\3131' '\3134' '\3137' '\3139' '\3141' '\3142' '\3145' '\3147' '\3148' '\314A' '\314B' '\314C' '\314D' '\314E';
  )CSS");

  ua_rules.Set(AtomicString("urdu"), R"CSS(
    system: extends persian;
  )CSS");

  return ua_rules;
}

const HashMap<AtomicString, String>& GetUACounterStyleRules() {
  using RuleMap = HashMap<AtomicString, String>;
  DEFINE_STATIC_LOCAL(RuleMap, ua_rules, (CollectUACounterStyleRules()));
  return ua_rules;
}

String GetUACounterStyleRuleText(const AtomicString& name) {
  StringBuilder builder;
  builder.Append("@counter-style ");
  builder.Append(name);
  builder.Append("{");
  builder.Append(GetUACounterStyleRules().at(name));
  builder.Append("}");
  return builder.ReleaseString();
}

}  // namespace

// static
CounterStyleMap* CounterStyleMap::CreateUACounterStyleMap() {
  CounterStyleMap* map =
      MakeGarbageCollected<CounterStyleMap>(nullptr, nullptr);
  // For UA counter style map, we only insert the names now, and defer the
  // creation of the CounterStyle objects until requested, so that we don't
  // waste memory on unused rules.
  for (const AtomicString& name : GetUACounterStyleRules().Keys()) {
    map->counter_styles_.Set(name, nullptr);
  }
  return map;
}

// static
CounterStyleMap* CounterStyleMap::GetUACounterStyleMap() {
  DEFINE_STATIC_LOCAL(Persistent<CounterStyleMap>, ua_counter_style_map,
                      (CreateUACounterStyleMap()));
  return ua_counter_style_map;
}

CounterStyle& CounterStyleMap::CreateUACounterStyle(const AtomicString& name) {
  const String& rule_text = GetUACounterStyleRuleText(name);
  const StyleSheetContents* sheet =
      CSSDefaultStyleSheets::ParseUASheet(rule_text);

  DCHECK_EQ(1u, sheet->ChildRules().size());
  DCHECK(IsA<StyleRuleCounterStyle>(sheet->ChildRules()[0].Get()));
  const auto* rule = To<StyleRuleCounterStyle>(sheet->ChildRules()[0].Get());

  CounterStyle* counter_style = CounterStyle::Create(*rule);
  DCHECK(counter_style) << "Predefined counter style " << name
                        << " has invalid symbols";
  counter_style->SetIsPredefined();
  if (IsPredefinedSymbolMarkerName(name)) {
    counter_style->SetIsPredefinedSymbolMarker();
  }
  counter_styles_.Set(name, counter_style);

  if (counter_style->HasUnresolvedExtends()) {
    ResolveExtendsFor(*counter_style);
  }
  if (counter_style->HasUnresolvedFallback()) {
    ResolveFallbackFor(*counter_style);
  }

  return *counter_style;
}

}  // namespace blink
