// META: title=Origin

'use strict';

//
// Construction and parsing:
//
test(t => {
  const origin = new Origin();
  assert_true(origin.opaque, "Origin should be opaque.");
  assert_equals(origin.toJSON(), "null", "toJSON() should return 'null'.");
}, "Default-constructed Origin is opaque.");

test(t => {
  const origin = new Origin("null");
  assert_true(origin.opaque, "Origin should be opaque.");
  assert_equals(origin.toJSON(), "null", "toJSON() should return 'null'.");
}, "Origin constructed with 'null' is opaque.");

test(t => {
  const origin = Origin.parse("null");
  assert_true(origin.opaque, "Origin should be opaque.");
  assert_equals(origin.toJSON(), "null", "toJSON() should return 'null'.");
}, "Origin parsed from 'null' is opaque.");

const tupleOrigins = [
  "http://site.example",
  "https://site.example",
  "https://site.example:123",
  "http://sub.site.example",
  "https://sub.site.example",
  "https://sub.site.example:123",
];

for (const originString of tupleOrigins) {
  test(t => {
    const origin = new Origin(originString);
    assert_false(origin.opaque, "Origin should not be opaque.");
    assert_equals(origin.toJSON(), originString, "toJSON() should return the serialized origin.");
  }, `Origin constructed from '${originString}' is a tuple origin.`);

  test(t => {
    const origin = Origin.parse(originString);
    assert_false(origin.opaque, "Origin should not be opaque.");
    assert_equals(origin.toJSON(), originString, "toJSON() should return the serialized origin.");
  }, `Origin parsed from '${originString}' is a tuple origin.`);

  test(t => {
    const a = new Origin(originString);
    const b = Origin.parse(originString);
    assert_true(a.isSameOrigin(b));
    assert_true(b.isSameOrigin(a));
  }, `Origins parsed and constructed from '${originString}' are equivalent.`);
}

//
// Comparison
//
test(t => {
  const opaqueA = new Origin("null");
  const opaqueB = new Origin("null");

  assert_true(opaqueA.isSameOrigin(opaqueA), "Opaque origin should be same-origin with itself.");
  assert_true(opaqueA.isSameSite(opaqueA), "Opaque origin should be same-site with itself.");
  assert_false(opaqueA.isSameOrigin(opaqueB), "Opaque origin should not be same-origin with another opaque origin.");
  assert_false(opaqueA.isSameSite(opaqueB), "Opaque origin should not be same-site with another opaque origin.");
}, "Comparison of opaque origins.");

test(t => {
  const a = new Origin("https://a.example");
  const a_a = new Origin("https://a.a.example");
  const b_a = new Origin("https://b.a.example");
  const b = new Origin("https://b.example");
  const b_b = new Origin("https://b.b.example");

  assert_true(a.isSameOrigin(a), "Origin should be same-origin with itself.");
  assert_false(a.isSameOrigin(a_a), "Origins with different subdomains should not be same-origin.");
  assert_false(a.isSameOrigin(b_a), "Origins with different subdomains should not be same-origin.");
  assert_false(a.isSameOrigin(b), "Origins with different domains should not be same-origin.");
  assert_false(a.isSameOrigin(b_b), "Origins with different domains should not be same-origin.");

  assert_true(a.isSameSite(a), "Origin should be same-site with itself.");
  assert_true(a.isSameSite(a_a), "Origins with same registrable domain should be same-site.");
  assert_true(a.isSameSite(b_a), "Origins with same registrable domain should be same-site.");
  assert_false(a.isSameSite(b), "Origins with different registrable domains should not be same-site.");
  assert_false(a.isSameSite(b_b), "Origins with different registrable domains should not be same-site.");

  assert_true(a_a.isSameSite(a), "Origins with same registrable domain should be same-site.");
  assert_true(a_a.isSameSite(a_a), "Origin should be same-site with itself.");
  assert_true(a_a.isSameSite(b_a), "Origins with same registrable domain should be same-site.");
  assert_false(a_a.isSameSite(b), "Origins with different registrable domains should not be same-site.");
  assert_false(a_a.isSameSite(b_b), "Origins with different registrable domains should not be same-site.");
}, "Comparison of tuple origins.");

//
// Invalid
//
const invalidOrigins = [
  "",
  "invalid",
  "about:blank",
  "https://trailing.slash/",
  "https://user:pass@site.example",
  "https://very.long.port:123456789",
];

for (const invalid of invalidOrigins) {
  test(t => {
    assert_throws_js(TypeError, () => new Origin(invalid), "Constructor should throw TypeError for invalid origin.");
  }, `Origin constructor throws for '${invalid}'.`);

  test(t => {
    assert_equals(Origin.parse(invalid), null, "parse() should return null for invalid origin.");
  }, `Origin.parse returns null for '${invalid}'.`);
}
