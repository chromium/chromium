description("Canonicalization of IPv4 addresses.");

cases = [ 
  [".", ""],

  // Regular IP addresses in different bases.
  ["192.168.0.1", "192.168.0.1"],
  ["0300.0250.00.01", "192.168.0.1"],
  ["0xC0.0Xa8.0x0.0x1", "192.168.0.1"],

  // Non-IP addresses due to invalid characters.
  ["192.168.9.com", ""],

  // Invalid characters for the base should be rejected.
  ["19a.168.0.1", ""],
  ["0308.0250.00.01", ""],
  ["0xCG.0xA8.0x0.0x1", ""],

  // If there are not enough components, the last one should fill them out.
  ["192", "0.0.0.192"],
  ["0xC0a80001", "192.168.0.1"],
  ["030052000001", "192.168.0.1"],
  ["000030052000001", "192.168.0.1"],
  ["192.168", "192.0.0.168"],
  ["192.0x00A80001", "192.168.0.1"],
  ["0xc0.052000001", "192.168.0.1"],
  ["192.168.1", "192.168.0.1"],

  // Too many components means not an IP address.
  ["192.168.0.0.1", ""],

  // We allow a single trailing dot.
  ["192.168.0.1.", "192.168.0.1"],
  ["192.168.0.1. hello", ""],
  ["192.168.0.1..", ""],

  // Two dots in a row means not an IP address.
  ["192.168..1", ""],

  // Any numerical overflow should be marked as BROKEN.
  ["0x100.0", ""],
  ["0x100.0.0", ""],
  ["0x100.0.0.0", ""],
  ["0.0x100.0.0", ""],
  ["0.0.0x100.0", ""],
  ["0.0.0.0x100", ""],
  ["0.0.0x10000", ""],
  ["0.0x1000000", ""],
  ["0x100000000", ""],

  // Repeat the previous tests, minus 1, to verify boundaries.
  ["0xFF.0", "255.0.0.0"],
  ["0xFF.0.0", "255.0.0.0"],
  ["0xFF.0.0.0", "255.0.0.0"],
  ["0.0xFF.0.0", "0.255.0.0"],
  ["0.0.0xFF.0", "0.0.255.0"],
  ["0.0.0.0xFF", "0.0.0.255"],
  ["0.0.0xFFFF", "0.0.255.255"],
  ["0.0xFFFFFF", "0.255.255.255"],
  ["0xFFFFFFFF", "255.255.255.255"],

  // Old trunctations tests.  They're all "BROKEN" now.
  ["276.256.0xf1a2.077777", ""],
  ["192.168.0.257", ""],
  ["192.168.0xa20001", ""],
  ["192.015052000001", ""],
  ["0X12C0a80001", ""],
  ["276.1.2", ""],

  // Spaces should be rejected.
  ["192.168.0.1 hello", ""],

  // Very large numbers.
  ["0000000000000300.0x00000000000000fF.00000000000000001", "192.255.0.1"],
  ["0000000000000300.0xffffffffFFFFFFFF.3022415481470977", ""],

  // A number has no length limit, but long numbers can still overflow.
  ["00000000000000000001", "0.0.0.1"],
  ["0000000000000000100000000000000001", ""],

  // If a long component is non-numeric, it's a hostname, *not* a broken IP.
  ["0.0.0.000000000000000000z", ""],
  ["0.0.0.100000000000000000z", ""],

  // Truncation of all zeros should still result in 0.
  ["0.00.0x.0x0", "0.0.0.0"]
];

for (var i = 0; i < cases.length; ++i) {
  test_vector = cases[i][0];
  expected_result = cases[i][1];
  if (expected_result === "") {
    // The result of `canonicalize` should be same as the input if input is an
    // invalid URL.
    expected_result = test_vector;
  }
  shouldBe("canonicalize('http://" + test_vector + "/')",
           "'http://" + expected_result + "/'");
}
