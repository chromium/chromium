function testGridDefinitionsValues(element, columnValue, rowValue)
{
    window.element = element;
    var elementID = element.id || "element";
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('grid-template-columns')", columnValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('grid-template-rows')", rowValue);
}

function testGridDefinitionsSetJSValues(columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue)
{
    checkGridDefinitionsSetJSValues(true, columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue);
}

function testNonGridDefinitionsSetJSValues(columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue)
{
    checkGridDefinitionsSetJSValues(false, columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue);
}

function testGridLanesDefinitionsSetJSValues(isRowDirection, trackValue, computedTrackValue, jsTrackValue)
{
    window.element = document.createElement("div");
    document.body.appendChild(element);
    element.style.display = "grid-lanes";
    if (isRowDirection) {
        element.style.gridLanesDirection = "row";
    }
    element.style.width = "800px";
    element.style.height = "600px";
    element.style.justifyContent = "start";
    element.style.alignContent = "start";
    element.style.font = "10px Ahem"; // Used to resolve em font consistently.
    if (isRowDirection) {
       element.style.gridTemplateRows = trackValue;
       shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('grid-template-rows')", computedTrackValue || trackValue);
       shouldBeEqualToString("element.style.gridTemplateRows", jsTrackValue || trackValue);
    } else {
         element.style.gridTemplateColumns = trackValue;
         shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('grid-template-columns')", computedTrackValue || trackValue);
         shouldBeEqualToString("element.style.gridTemplateColumns", jsTrackValue || trackValue);
    }
    document.body.removeChild(element);
}

function checkGridDefinitionsSetJSValues(useGrid, columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue)
{
    window.element = document.createElement("div");
    document.body.appendChild(element);
    if (useGrid) {
        element.style.display = "grid";
        element.style.width = "800px";
        element.style.height = "600px";
        element.style.justifyContent = "start";
        element.style.alignContent = "start";
    }
    element.style.font = "10px Ahem"; // Used to resolve em font consistently.
    element.style.gridTemplateColumns = columnValue;
    element.style.gridTemplateRows = rowValue;
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('grid-template-columns')", computedColumnValue || columnValue);
    shouldBeEqualToString("element.style.gridTemplateColumns", jsColumnValue || columnValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('grid-template-rows')", computedRowValue || rowValue);
    shouldBeEqualToString("element.style.gridTemplateRows", jsRowValue || rowValue);
    document.body.removeChild(element);
}

function testGridDefinitionsSetBadJSValues(columnValue, rowValue)
{
    window.element = document.createElement("div");
    document.body.appendChild(element);
    element.style.gridTemplateColumns = columnValue;
    element.style.gridTemplateRows = rowValue;
    // We can't use testSetJSValues as element.style.gridTemplateRows returns "".
    testGridDefinitionsValues(element, "none", "none");
    document.body.removeChild(element);
}

function checkGridAutoFlowSetCSSValue(elementId, expectedValue)
{
    shouldBe("window.getComputedStyle(" + elementId + ", '').getPropertyValue('grid-auto-flow')", "'" + expectedValue + "'");
}

function checkGridAutoFlowSetJSValue(newValue, expectedStyleValue, expectedComputedStyleValue)
{
    element = document.createElement("div");
    document.body.appendChild(element);
    if (newValue)
        element.style.gridAutoFlow = newValue;
    shouldBe("element.style.gridAutoFlow", "'" + expectedStyleValue + "'");
    shouldBe("window.getComputedStyle(element, '').getPropertyValue('grid-auto-flow')", "'" + expectedComputedStyleValue + "'");
    document.body.removeChild(element);
}

function testGridAutoDefinitionsValues(element, computedRowValue, computedColumnValue)
{
    window.element = element;
    var elementID = element.id || "element";
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('grid-auto-rows')", computedRowValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('grid-auto-columns')", computedColumnValue);
}

function testGridPositionDefinitionsValues(
    element, computedRowStart, computedRowEnd, computedColumnStart,
    computedColumnEnd) {
  assert_equals(
      window.getComputedStyle(element, '').getPropertyValue('grid-row-start'),
      computedRowStart, 'row-start');
  assert_equals(
      window.getComputedStyle(element, '').getPropertyValue('grid-row-end'),
      computedRowEnd, 'row-end');
  assert_equals(
      window.getComputedStyle(element, '')
          .getPropertyValue('grid-column-start'),
      computedColumnStart, 'column-start');
  assert_equals(
      window.getComputedStyle(element, '').getPropertyValue('grid-column-end'),
      computedColumnEnd, 'column-end');
}
