description(
"String.replace(&hellip;) test"
);

var testString = "It's the end of the world as we know it, and I feel fine.";
shouldBe("testString",
         "\"It's the end of the world as we know it, and I feel fine.\"");
shouldBe("testString.replace('end','BEGINNING')",
         "\"It's the BEGINNING of the world as we know it, and I feel fine.\"");
shouldBe("testString.replace(/[aeiou]/gi,'-')",
         "\"-t's th- -nd -f th- w-rld -s w- kn-w -t, -nd - f--l f-n-.\"");
shouldBe("testString.replace(/[aeiou]/gi, function Capitalize(s){ return s.toUpperCase(); })", 
         "\"It's thE End Of thE wOrld As wE knOw It, And I fEEl fInE.\"");
// See https://crbug.com/569139.
shouldBe("testString.replace(/([aeiou])([a-z])/g, function Capitalize(){ return RegExp.$1.toUpperCase()+RegExp.$2; })",
         "\"It's the Ind In the wInld In we knIn In, Ind I fInl fIne.\"");
shouldBe("testString.replace(/([aeiou])([a-z])/g, function Capitalize(orig,re1,re2) { return re1.toUpperCase()+re2; })",
        "\"It's the End Of the wOrld As we knOw It, And I fEel fIne.\"");
shouldBe("testString.replace(/(.*)/g, function replaceWithDollars(matchGroup) { return '$1'; })", "\"$1$1\"");
shouldBe("testString.replace(/(.)(.*)/g, function replaceWithMultipleDollars(matchGroup) { return '$1$2'; })", "\"$1$2\"");
shouldBe("testString.replace(/(.)(.*)/, function checkReplacementArguments() { return arguments.length; })", "\"5\"");

// replace with a global regexp should set lastIndex to zero; if read-only this should throw.
// If the regexp is not global, lastIndex is not modified.
var re;
var replacer;
function testReplace(_re, readonly)
{
    re = _re;
    re.lastIndex = 3;
    if (readonly)
        re = Object.defineProperty(re, 'lastIndex', {writable:false});
    return '0x1x2'.replace(re, replacer);
}

replacer = 'y';
shouldBe("testReplace(/x/g, false)", '"0y1y2"');
shouldThrow("testReplace(/x/g, true)");
shouldBe("testReplace(/x/, false)", '"0y1x2"');
shouldBe("testReplace(/x/, true)", '"0y1x2"');
shouldBe("testReplace(/x/g, false); re.lastIndex", '0');
shouldThrow("testReplace(/x/g, true); re.lastIndex");
shouldBe("testReplace(/x/, false); re.lastIndex", '3');
shouldBe("testReplace(/x/, true); re.lastIndex", '3');

replacer = function() { return 'y'; };
shouldBe("testReplace(/x/g, false)", '"0y1y2"');
shouldThrow("testReplace(/x/g, true)");
shouldBe("testReplace(/x/, false)", '"0y1x2"');
shouldBe("testReplace(/x/, true)", '"0y1x2"');
shouldBe("testReplace(/x/g, false); re.lastIndex", '0');
shouldThrow("testReplace(/x/g, true); re.lastIndex");
shouldBe("testReplace(/x/, false); re.lastIndex", '3');
shouldBe("testReplace(/x/, true); re.lastIndex", '3');

replacer = function() { "use strict"; return ++re.lastIndex; };
shouldBe("testReplace(/x/g, false)", '"01122"');
shouldThrow("testReplace(/x/g, true)");
shouldBe("testReplace(/x/, false)", '"041x2"');
shouldThrow("testReplace(/x/, true)");
shouldBe("testReplace(/x/g, false); re.lastIndex", '2');
shouldThrow("testReplace(/x/g, true); re.lastIndex");
shouldBe("testReplace(/x/, false); re.lastIndex", '4');
shouldThrow("testReplace(/x/, true); re.lastIndex");

var replacerCalled = false;
replacer = function() { replacerCalled = true; };
shouldBeTrue("try { testReplace(/x/g, false); throw 0; } catch (e) { }; replacerCalled;");
var replacerCalled = false;
replacer = function() { replacerCalled = true; };
shouldBeFalse("try { testReplace(/x/g, true); throw 0; } catch (e) { }; replacerCalled;");
