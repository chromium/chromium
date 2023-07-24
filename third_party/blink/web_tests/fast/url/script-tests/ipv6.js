description("Canonicalization of IPv6 addresses.");

cases = [ 
    ["[", ""],
    ["[:", ""],
    ["]", ""],
    [":]", ""],
    ["[]", ""],
    ["[:]", ""],

    // Regular IP address is invalid without bounding '[' and ']'.
    ["2001:db8::1", ""],
    ["[2001:db8::1", ""],
    ["2001:db8::1]", ""],

    // Regular IP addresses.
    ["[::]", "[::]"],
    ["[::1]", "[::1]"],
    ["[1::]", "[1::]"],
    ["[::192.168.0.1]", "[::c0a8:1]"],
    ["[::ffff:192.168.0.1]", "[::ffff:c0a8:1]"],

    // Leading zeros should be stripped.
    ["[000:01:02:003:004:5:6:007]", "[0:1:2:3:4:5:6:7]"],

    // Upper case letters should be lowercased.
    ["[A:b:c:DE:fF:0:1:aC]", "[a:b:c:de:ff:0:1:ac]"],

    // The same address can be written with different contractions, but should
    // get canonicalized to the same thing.
    ["[1:0:0:2::3:0]", "[1::2:0:0:3:0]"],
    ["[1::2:0:0:3:0]", "[1::2:0:0:3:0]"],

    // IPv4 addresses
    // Only mapped and compat addresses can have IPv4 syntax embedded.
    ["[::eeee:192.168.0.1]", ""],
    ["[2001::192.168.0.1]", ""],
    ["[1:2:192.168.0.1:5:6]", ""],

    // IPv4 with last component missing.
    ["[::ffff:192.1.2]", ""],

    // IPv4 using hex.
    // FIXME: Should this format be disallowed?
    ["[::ffff:0xC0.0Xa8.0x0.0x1]", "[::ffff:c0a8:1]"],

    // There may be zeros surrounding the "::" contraction.
    ["[0:0::0:0:8]", "[::8]"],

    ["[2001:db8::1]", "[2001:db8::1]"],

    // Can only have one "::" contraction in an IPv6 string literal.
    ["[2001::db8::1]", ""],

    // No more than 2 consecutive ':'s.
    ["[2001:db8:::1]", ""],
    ["[:::]", ""],

    // Non-IP addresses due to invalid characters.
    ["[2001::.com]", ""],

    // Too many components means not an IP address.  Similarly with too few if using IPv4 compat or mapped addresses.
    ["[::192.168.0.0.1]", ""],
    ["[::ffff:192.168.0.0.1]", ""],
    ["[1:2:3:4:5:6:7:8:9]", ""],

    // Too many bits (even though 8 comonents, the last one holds 32 bits).
    ["[0:0:0:0:0:0:0:192.168.0.1]", ""],

    // Too many bits specified -- the contraction would have to be zero-length
    // to not exceed 128 bits.
    ["[1:2:3:4:5:6::192.168.0.1]", ""],

    // The contraction is for 16 bits of zero. RFC 5952, Section 4.2.2.
    ["[1:2:3:4:5:6::8]", "[1:2:3:4:5:6:0:8]"],

    // Cannot have a trailing colon.
    ["[1:2:3:4:5:6:7:8:]", ""],
    ["[1:2:3:4:5:6:192.168.0.1:]", ""],

    // Cannot have negative numbers.
    ["[-1:2:3:4:5:6:7:8]", ""],

    // Scope ID -- the URL may contain an optional ["%" <scope_id>] section.
    // The scope_id should be included in the canonicalized URL, and is an
    // unsigned decimal number.

    // Don't allow scope-id
    ["[1::%1]", ""],
    ["[1::%eth0]", ""],
    ["[1::%]", ""],
    ["[%]", ""],
    ["[::%:]", ""],

    // Don't allow leading or trailing colons.
    ["[:0:0::0:0:8]", ""],
    ["[0:0::0:0:8:]", ""],
    ["[:0:0::0:0:8:]", ""],

    // Two dots in a row means not an IP address.
    ["[::192.168..1]", ""],

    // Spaces should be rejected.
    ["[::1 hello]", ""]
];

for (var i = 0; i < cases.length; ++i) {
  test_vector = cases[i][0];
  expected_result = cases[i][1];
  if (expected_result === "")
    expected_result = test_vector;
  shouldBe("canonicalize('http://" + test_vector + "/')",
           "'http://" + expected_result + "/'");
}
