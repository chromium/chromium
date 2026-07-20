description("This test checks that background-size:inherit properly inherits all values.");

function test(value)
{
    var div = document.createElement("div");
    div.setAttribute("style", value);
    document.body.appendChild(div);
    
    var child = document.createElement("div");
    child.setAttribute("style", "background-size: inherit;");
    div.appendChild(child);
    
    var result = getComputedStyle(child, null).getPropertyValue("background-size");
    document.body.removeChild(div);
    return result;
}

shouldBe('test("background-size: contain;")', '"contain"');
shouldBe('test("background-size: cover;")', '"cover"');
shouldBe('test("background-size: 100 100;")', '"auto"');
shouldBe('test("background-size: 100px 100px;")', '"100px 100px"');
shouldBe('test("background-size: auto 50px;")', '"auto 50px"');
shouldBe('test("background-size: 50px auto;")', '"50px auto"');
shouldBe('test("background-size: auto auto;")', '"auto"');
shouldBe('test("background-size: 30% 20%;")', '"30% 20%"');
shouldBe('test("background-size: 4em auto;")', '"64px auto"');
shouldBe('test("background-size: 5em ;")', '"80px auto"');
shouldBe('test("-webkit-background-size: 5em ;")', '"80px 80px"');

shouldBe('test("background-size: 100px 100px 100px;")', '"auto"');
shouldBe('test("background-size: coconut;")', '"auto"');
