description('Test that setting and getting grid-template-columns and grid-template-rows works as expected');

debug("Test getting grid-template-columns and grid-template-rows set through CSS");
testGridDefinitionsValues(document.getElementById("gridWithNoneElement"), "none", "none");
testGridDefinitionsValues(document.getElementById("gridWithFixedElement"), "10px", "15px");
testGridDefinitionsValues(document.getElementById("gridWithPercentElement"), "400px", "150px");
testGridDefinitionsValues(document.getElementById("gridWithPercentWithoutSize"), "0px", "0px");
testGridDefinitionsValues(document.getElementById("gridWithPercentWithoutSizeWithChildren"), "3.5px", "4px");
testGridDefinitionsValues(document.getElementById("gridWithAutoElement"), "0px", "0px");
testGridDefinitionsValues(document.getElementById("gridWithAutoWithoutSizeElement"), "0px", "0px");
testGridDefinitionsValues(document.getElementById("gridWithAutoWithChildrenElement"), "7px", "16px");
testGridDefinitionsValues(document.getElementById("gridWithEMElement"), "100px", "150px");
testGridDefinitionsValues(document.getElementById("gridWithViewPortPercentageElement"), "64px", "60px");
testGridDefinitionsValues(document.getElementById("gridWithMinMaxElement"), "80px", "300px");
testGridDefinitionsValues(document.getElementById("gridWithMinContentElement"), "0px", "0px");
testGridDefinitionsValues(document.getElementById("gridWithMinContentWithChildrenElement"), "17px", "16px");
testGridDefinitionsValues(document.getElementById("gridWithMaxContentElement"), "0px", "0px");
testGridDefinitionsValues(document.getElementById("gridWithMaxContentWithChildrenElement"), "17px", "16px");
testGridDefinitionsValues(document.getElementById("gridWithFractionElement"), "800px", "600px");
testGridDefinitionsValues(document.getElementById("gridWithCalcElement"), "150px", "75px");
testGridDefinitionsValues(document.getElementById("gridWithCalcComplexElement"), "550px", "465px");
testGridDefinitionsValues(document.getElementById("gridWithCalcInsideMinMaxElement"), "80px", "300px");
testGridDefinitionsValues(document.getElementById("gridWithCalcComplexInsideMinMaxElement"), "415px", "300px");
testGridDefinitionsValues(document.getElementById("gridWithAutoInsideMinMaxElement"), "20px", "16px");
testGridDefinitionsValues(document.getElementById("gridWithFitContentFunctionElement"), "7px", "16px");

debug("");
debug("Test getting wrong values for grid-template-columns and grid-template-rows through CSS (they should resolve to the default: 'none')");
var gridWithFitContentElement = document.getElementById("gridWithFitContentElement");
testGridDefinitionsValues(gridWithFitContentElement, "none", "none");

var gridWithFitAvailableElement = document.getElementById("gridWithFitAvailableElement");
testGridDefinitionsValues(gridWithFitAvailableElement, "none", "none");

debug("");
debug("Test the initial value");
var element = document.createElement("div");
document.body.appendChild(element);
testGridDefinitionsValues(element, "none", "none");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-rows')", "'none'");

debug("");
debug("Test getting and setting grid-template-columns and grid-template-rows through JS");
testGridDefinitionsSetJSValues("18px", "66px");
testGridDefinitionsSetJSValues("55%", "40%", "440px", "240px");
testGridDefinitionsSetJSValues("auto", "auto", "0px", "0px");
testGridDefinitionsSetJSValues("10vw", "25vh", "80px", "150px");
testGridDefinitionsSetJSValues("min-content", "min-content", "0px", "0px");
testGridDefinitionsSetJSValues("max-content", "max-content", "0px", "0px");
testGridDefinitionsSetJSValues("fit-content(100px)", "fit-content(25%)", "0px", "0px");

debug("");
debug("Test getting and setting grid-template-columns and grid-template-rows to minmax() values through JS");
testGridDefinitionsSetJSValues("minmax(55%, 45px)", "minmax(30px, 40%)", "440px", "240px");
testGridDefinitionsSetJSValues("minmax(22em, 8vh)", "minmax(10vw, 5em)", "220px", "80px");
testGridDefinitionsSetJSValues("minmax(min-content, 8vh)", "minmax(10vw, min-content)", "48px", "80px");
testGridDefinitionsSetJSValues("minmax(22em, max-content)", "minmax(max-content, 5em)", "220px", "50px");
testGridDefinitionsSetJSValues("minmax(min-content, max-content)", "minmax(max-content, min-content)", "0px", "0px");
testGridDefinitionsSetJSValues("minmax(auto, max-content)", "minmax(10vw, auto)", "0px", "80px");
// Unit comparison should be case-insensitive.
testGridDefinitionsSetJSValues("3600Fr", "154fR", "800px", "600px", "3600fr", "154fr");
// Float values are allowed.
testGridDefinitionsSetJSValues("3.1459fr", "2.718fr", "800px", "600px");
// A leading '+' is allowed.
testGridDefinitionsSetJSValues("+3fr", "+4fr", "800px", "600px", "3fr", "4fr");
// Flex factor values can be zero.
testGridDefinitionsSetJSValues("0fr", ".0fr", "0px", "0px", "0fr", "0fr");
testGridDefinitionsSetJSValues("minmax(auto, 0fr)", "minmax(auto, .0fr)", "0px", "0px", "minmax(auto, 0fr)", "minmax(auto, 0fr)");

debug("");
debug("Test getting and setting grid-template-columns and grid-template-rows to calc() values through JS");
testGridDefinitionsSetJSValues("calc(150px)", "calc(75px)", "150px", "75px");
testGridDefinitionsSetJSValues("calc(50% - 30px)", "calc(10% + 75px)", "370px", "135px");
testGridDefinitionsSetJSValues("minmax(25%, calc(30px))", "minmax(calc(75%), 40px)", "200px", "450px", "minmax(25%, calc(30px))", "minmax(calc(75%), 40px)");
testGridDefinitionsSetJSValues("minmax(10%, calc(30px + 10%))", "minmax(calc(25% - 50px), 200px)", "110px", "200px", "minmax(10%, calc(10% + 30px))", "minmax(calc(25% - 50px), 200px)");

debug("");
debug("Test setting grid-template-columns and grid-template-rows to bad values through JS");
// No comma and only 1 argument provided.
testGridDefinitionsSetBadJSValues("minmax(10px 20px)", "minmax(10px)")
// Nested minmax and only 2 arguments are allowed.
testGridDefinitionsSetBadJSValues("minmax(minmax(10px, 20px), 20px)", "minmax(10px, 20px, 30px)");
// No breadth value and no comma.
testGridDefinitionsSetBadJSValues("minmax()", "minmax(30px 30% 30em)");
testGridDefinitionsSetBadJSValues("-2fr", "3ffr");
testGridDefinitionsSetBadJSValues("-2.05fr", "+-3fr");
testGridDefinitionsSetBadJSValues("1f", "1r");
// A dimension doesn't allow spaces between the number and the unit.
testGridDefinitionsSetBadJSValues(".1 fr", "13 fr");
testGridDefinitionsSetBadJSValues("7.-fr", "-8,0fr");
// Negative values are not allowed.
testGridDefinitionsSetBadJSValues("-1px", "-6em");
testGridDefinitionsSetBadJSValues("minmax(-1%, 32%)", "minmax(2vw, -6em)");
// Invalid expressions with calc
testGridDefinitionsSetBadJSValues("calc(16px 30px)", "calc(25px + auto)");
testGridDefinitionsSetBadJSValues("minmax(min-content, calc())", "calc(10%(");
// Forward slash not allowed if not part of a shorthand
testGridDefinitionsSetBadJSValues("10px /", "15px /");
// Flexible lengths are invalid on the min slot of minmax().
testGridDefinitionsSetBadJSValues("minmax(0fr, 100px)", "minmax(.0fr, 200px)");
testGridDefinitionsSetBadJSValues("minmax(1fr, 100px)", "minmax(2.5fr, 200px)");
testGridDefinitionsSetBadJSValues("fit-content(-10em)", "fit-content(-2px)");
testGridDefinitionsSetBadJSValues("fit-content(10px 2%)", "fit-content(5% 10em)");
testGridDefinitionsSetBadJSValues("fit-content(max-content)", "fit-content(min-content)");
testGridDefinitionsSetBadJSValues("fit-content(auto)", "fit-content(3fr)");
testGridDefinitionsSetBadJSValues("fit-content(repeat(2, 100px))", "fit-content(repeat(auto-fit), 1%)");
testGridDefinitionsSetBadJSValues("fit-content(fit-content(10px))", "fit-content(fit-content(30%))");
testGridDefinitionsSetBadJSValues("fit-content([a] 100px)", "fit-content(30px [b c])");

debug("");
debug("Test setting grid-template-columns and grid-template-rows back to 'none' through JS");
testGridDefinitionsSetJSValues("18px", "66px");
testGridDefinitionsSetJSValues("none", "none");

function testInherit()
{
    var parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style.gridTemplateColumns = "50px [last]";
    parentElement.style.gridTemplateRows = "[first] 101%";

    element = document.createElement("div");
    parentElement.appendChild(element);
    element.style.display = "grid";
    element.style.height = "100px";
    element.style.gridTemplateColumns = "inherit";
    element.style.gridTemplateRows = "inherit";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-columns')", "'50px [last]'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-rows')", "'[first] 101px'");

    document.body.removeChild(parentElement);
}
debug("");
debug("Test setting grid-template-columns and grid-template-rows to 'inherit' through JS");
testInherit();

function testInitial()
{
    element = document.createElement("div");
    document.body.appendChild(element);
    element.style.display = "grid";
    element.style.width = "300px";
    element.style.height = "150px";
    element.style.gridTemplateColumns = "150% [last]";
    element.style.gridTemplateRows = "[first] 1fr";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-columns')", "'450px [last]'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-rows')", "'[first] 150px'");

    element.style.display = "grid";
    element.style.gridTemplateColumns = "initial";
    element.style.gridTemplateRows = "initial";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-columns')", "'none'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-template-rows')", "'none'");

    document.body.removeChild(element);
}
debug("");
debug("Test setting grid-template-columns and grid-template-rows to 'initial' through JS");
testInitial();
