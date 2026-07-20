description("This tests checks that all of the input values for background-size parse correctly.");

function test(value)
{
    var div = document.createElement("div");
    div.setAttribute("style", value);
    document.body.appendChild(div);
    
    var result = div.style.getPropertyValue(value.substring(0, value.indexOf(":")));
    document.body.removeChild(div);
    return result;
}

shouldBe('test("background-size: contain;")', '"contain"');
shouldBe('test("background-size: cover;")', '"cover"');
shouldBeEqualToString('test("background-size: 100 100;")', '');
shouldBe('test("background-size: 100px 100px;")', '"100px 100px"');
shouldBe('test("background-size: auto 50px;")', '"auto 50px"');
shouldBe('test("background-size: 50px auto;")', '"50px auto"');
shouldBe('test("background-size: auto auto;")', '"auto"');
shouldBe('test("background-size: 30% 20%;")', '"30% 20%"');
shouldBe('test("background-size: 4em auto;")', '"4em auto"');
shouldBe('test("background-size: 5em;")', '"5em auto"');
shouldBe('test("-webkit-background-size: 5em ;")', '"5em 5em"');

shouldBeEqualToString('test("background-size: 100px 100px 100px;")', '');
shouldBeEqualToString('test("background-size: coconut;")', '');

shouldBeEqualToString('test("background-size: 100px,;")', '');
shouldBe('test("background-size: 100px, 50%;")', '"100px auto, 50% auto"');
shouldBe('test("-webkit-background-size: 100px, 50%;")', '"100px, 50% 50%"');
shouldBe('test("background-size: 50% 100px, 2em, 100px 50%;")',
         '"50% 100px, 2em auto, 100px 50%"');
shouldBe('test("-webkit-background-size: 50% 100px, 2em, 100px 50%;")', '"50% 100px, 2em, 100px 50%"');
