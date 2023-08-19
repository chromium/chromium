description("IDNA2003 handling in domain name labels.");

debug("The PASS/FAIL results of this test are set to the behavior in IDNA2003.");

cases = [ 
  // For IDNA Compatibility test material see
  // http://www.unicode.org/reports/tr46/
  // 1) Deviant character tests (deviant from IDNA2008)
  // U+00DF normalizes to "ss" during IDNA2003's mapping phase
  ["fa\u00DF.de","fass.de"],
  // The ς U+03C2 GREEK SMALL LETTER FINAL SIGMA is mapped to U+03C3
  ["\u03B2\u03CC\u03BB\u03BF\u03C2.com","xn--nxasmq6b.com"],
  // The ZWJ U+200D ZERO WIDTH JOINER is mapped to nothing.
  ["\u0DC1\u0DCA\u200D\u0DBB\u0DD3.com","xn--10cl1a0b.com"],
  // The ZWNJ U+200C ZERO WIDTH NON-JOINER is mapped to nothing.
  ["\u0646\u0627\u0645\u0647\u200C\u0627\u06CC.com","xn--mgba3gch31f.com"],
  // 2) Normalization tests
  ["www.loo\u0138out.net","www.xn--looout-5bb.net"],
  ["\u15EF\u15EF\u15EF.lookout.net","xn--1qeaa.lookout.net"],
  ["www.lookout.\u0441\u043E\u043C","www.lookout.xn--l1adi"],
  // Invalid URL.
  ["www.lookout.net\uFF1A80"],
  // Invalid URL.
  ["www\u2025lookout.net"],
  ["www.lookout\u2027net","www.xn--lookoutnet-406e"],
  // using Latin letter kra ‘ĸ’ in domain
  ["www.loo\u0138out.net","www.xn--looout-5bb.net"],
  // "http://www.lookout.net\u2A7480/" is invalid URL.
  // \u2A74 decomposes into ::=
  ["www.lookout.net\u2A7480"],
  // U+0341; COMBINING ACUTE TONE MARK is normalized to U+0301
  ["lookout\u0341.net","xn--lookout-zge.net"],
  // 3) Characters mapped away : See RFC 3454 B.1
  //   U+2060 WORD JOINER is mapped to nothing.
  ["look\u2060out.net","lookout.net"],
  //   U+FEFF ZERO WIDTH NO-BREAK SPACE is mapped to nothing.
  ["look\uFEFFout.net","lookout.net"],
  //   U+FE00 VARIATION SELECTOR-1 is mapped to nothing.
  ["look\uFE00out.net","lookout.net"],
  // 4) Prohibited code points
  //   Using prohibited high-ASCII \u00A0
  ["www\u00A0.lookout.net","www%20.lookout.net"],
  //   using prohibited non-ASCII space chars 1680 (Ogham space mark)
  ["\u1680lookout.net"],
  //   Using prohibited lower ASCII control character \u001F
  ["\u001Flookout.net"],
  //   Using prohibited U+06DD ARABIC END OF AYAH
  ["look\u06DDout.net"],
  //   Using prohibited U+180E MONGOLIAN VOWEL SEPARATOR
  ["look\u180Eout.net"],
  //   Using prohibited Non-character code points 1FFFE [NONCHARACTER CODE POINTS]
  ["look\uD83F\uDFFEout.net"],
  //   Using prohibited U+DEAD half surrogate code point
  // FIXME: ["look\uDEADout.net","look%ED%BA%ADout.net"],
  //   Using prohibited Inappropriate for plain text U+FFFA; INTERLINEAR ANNOTATION SEPARATOR
  ["look\uFFFAout.net"],
  //   Using prohibited Inappropriate for canonical representation 2FF0-2FFB; [IDEOGRAPHIC DESCRIPTION CHARACTERS]
  ["look\u2FF0out.net"],
  //   Using prohibited Change display properties or are deprecated 202E; RIGHT-TO-LEFT OVERRIDE
  ["look\u202Eout.net"],
  //   Using prohibited Change display properties or are deprecated 206B; ACTIVATE SYMMETRIC SWAPPING
  ["look\u206Bout.net"],
  //   Using prohibited Tagging characters E0001; LANGUAGE TAG
  ["look\uDB40\uDC01out.net"],
  //   Using prohibited Tagging characters E0020-E007F; [TAGGING CHARACTERS]
  ["look\uDB40\uDC20out.net"],
  //   Using prohibited Characters with bidirectional property 05BE
  ["look\u05BEout.net"]
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
