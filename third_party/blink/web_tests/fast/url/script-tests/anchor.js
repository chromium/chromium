description("Test URLs that have an anchor.");

cases = [ 
  ["hello, world", "hello,%20world"],
  ["\xc2\xa9", "%C3%82%C2%A9"],
  ["\ud800\udf00ss", "%F0%90%8C%80ss"],
  ["%41%a", "%41%a"],
  ["\\ud800\\u597d", "%EF%BF%BD%E5%A5%BD"],
  ["a\\uFDD0", "a%EF%B7%90"],
  ["asdf#qwer", "asdf#qwer"],
  ["#asdf", "#asdf"],
  ["a\\nb\\rc\\td", "abcd"],
];

for (var i = 0; i < cases.length; ++i) {
  shouldBe("canonicalize('http://www.example.com/#" + cases[i][0] + "')",
           "'http://www.example.com/#" + cases[i][1] + "'");
}
