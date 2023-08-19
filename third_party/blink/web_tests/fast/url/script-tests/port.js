description("Test URLs that have a port number.");

cases = [ 
  // Invalid input should be copied w/ failure.
  ["as df", "as df"],
  ["-2", ":-2"],
  // Default port should be omitted.
  ["80", ""],
  ["8080", ":8080"],
  // Empty ports (just a colon) should also be removed
  ["", ""],
  // Code point with a numeric value U+1369 ETHIOPIC DIGIT ONE
  ["\u1369", ":\u1369"],
  // Code point with a numerical mapping and value U+1D7D6 MATHEMATICAL BOLD DIGIT EIGHT
  ["\uD835\uDFD6", ":\uD835\uDFD6"],
];

for (var i = 0; i < cases.length; ++i) {
  shouldBe("canonicalize('http://www.example.com:" + cases[i][0] + "/')",
           "'http://www.example.com" + cases[i][1] + "/'");
}

// Unspecified port should mean always keep the port.
shouldBe("canonicalize('foobar://www.example.com:80/')",
         "'foobar://www.example.com:80/'");
