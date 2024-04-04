description("IDNA2008 handling in domain name labels.");

debug("The PASS/FAIL results of this test are set to the behavior in IDNA2008.");

cases = [ 
  // For IDNA Compatibility test material see
  // http://www.unicode.org/reports/tr46/
  // http://www.unicode.org/Public/idna/latest/IdnaMappingTable.txt 
  // We are testing disallowed, ignored, mapped, deviant, and valid cases.
  // 1) Deviant character tests (deviant processing behavior from IDNA2003)
  ["B\u00FCcher.de","xn--bcher-kva.de"],
  // The ß U+00DF LATIN SMALL LETTER SHARP S does NOT normalize to "ss" like it does during IDNA2003's mapping phase
  ["fa\u00DF.de","xn--fa-hia.de"],
  // The ς U+03C2 GREEK SMALL LETTER FINAL SIGMA using βόλος.com
  ["\u03B2\u03CC\u03BB\u03BF\u03C2.com","xn--nxasmm1c.com"],
  // The ZWJ U+200D ZERO WIDTH JOINER
  ["\u0DC1\u0DCA\u200D\u0DBB\u0DD3.com","xn--10cl1a0b660p.com"],
  // The ZWNJ U+200C ZERO WIDTH NON-JOINER
  ["\u0646\u0627\u0645\u0647\u200C\u0627\u06CC.com","xn--mgba3gch31f060k.com"],
  // U+2665 BLACK HEART SUIT
  ["\u2665.net","xn--g6h.net"],
  // 2) Disallowed characters in IDNA2008
  // U+0378 <reserved>
  ["\u0378.net"],
  ["\u04C0.com"],
  ["\uD87E\uDC68.com"],
  ["\u2183.com"],
  // 3) Ignored characters should be removed * security risk
  // U+034F COMBINING GRAPHEME JOINER
  ["look\u034Fout.net","lookout.net"],
  // 4) Mapped characters.
  ["gOoGle.com","google.com"],
  // U+09DC is normalized to U+09A1, U+09BC before being turned to punycode.
  ["\u09dc.com","xn--15b8c.com"],
  // U+1E9E (ẞ; uppercase of U+00DF, ß).
  ["\u1E9E.com","xn--zca.com"],
  // 5) Validity FAIL cases - these should each cause an error.
  ["-foo.bar.com",""],
  ["foo-.bar.com",""],
  ["ab--cd.com",""],
  ["xn--0.com",""],
  ["foo\u0300.bar.com","foo%CC%80.bar.com"]
];

for (var i = 0; i < cases.length; ++i) {
  test_vector = cases[i][0];
  expected_result = cases[i][1];
  if (!expected_result) {
    // The result of `canonicalize` should be same as the input if input is an
    // invalid URL.
    expected_result = test_vector;
  }
  shouldBe("canonicalize('http://" + test_vector + "/')",
           "'http://" + expected_result + "/'");
}
