(function() {

function checkColumnRowValues(gridItem, columnValue, rowValue, columnStartValue, columnEndValue, rowStartValue, rowEndValue)
{
    this.gridItem = gridItem;
    var gridItemId = gridItem.id ? gridItem.id : "gridItem";

    shouldBeEqualToString("getComputedStyle(" + gridItemId + ", '').getPropertyValue('grid-column')", columnValue);
    shouldBeEqualToString("getComputedStyle(" + gridItemId + ", '').getPropertyValue('grid-row')", rowValue);
    if (columnStartValue) {
        shouldBeEqualToString("getComputedStyle(" + gridItemId + ", '').getPropertyValue('grid-column-start')", columnStartValue);
    }
    if (columnEndValue) {
        shouldBeEqualToString("getComputedStyle(" + gridItemId + ", '').getPropertyValue('grid-column-end')", columnEndValue);
    }
    if (rowStartValue) {
        shouldBeEqualToString("getComputedStyle(" + gridItemId + ", '').getPropertyValue('grid-row-start')", rowStartValue);
    }
    if (rowEndValue) {
        shouldBeEqualToString("getComputedStyle(" + gridItemId + ", '').getPropertyValue('grid-row-end')", rowEndValue);
    }
}

window.testColumnRowCSSParsing = function(id, columnValue, rowValue, columnStartValue, columnEndValue, rowStartValue, rowEndValue)
{
    var gridItem = document.getElementById(id);
    checkColumnRowValues(gridItem, columnValue, rowValue, columnStartValue, columnEndValue, rowStartValue, rowEndValue);
}

window.testColumnRowJSParsing = function(columnValue, rowValue, expectedColumnValue, expectedRowValue)
{
    var gridItem = document.createElement("div");
    var gridElement = document.getElementsByClassName("grid")[0];
    gridElement.appendChild(gridItem);
    gridItem.style.gridColumn = columnValue;
    gridItem.style.gridRow = rowValue;

    checkColumnRowValues(gridItem, expectedColumnValue, expectedRowValue);

    gridElement.removeChild(gridItem);
}

window.testColumnStartRowStartJSParsing = function(columnStartValue, rowStartValue, expectedColumnStartValue, expectedRowStartValue)
{
    var gridItem = document.createElement("div");
    var gridElement = document.getElementsByClassName("grid")[0];
    gridElement.appendChild(gridItem);
    gridItem.style.gridColumnStart = columnStartValue;
    gridItem.style.gridRowStart = rowStartValue;

    if (expectedColumnStartValue === undefined)
        expectedColumnStartValue = columnStartValue;
    if (expectedRowStartValue === undefined)
        expectedRowStartValue = rowStartValue;

    checkColumnRowValues(gridItem, expectedColumnStartValue, expectedRowStartValue);

    gridElement.removeChild(gridItem);
}

window.testColumnEndRowEndJSParsing = function(columnEndValue, rowEndValue, expectedColumnEndValue, expectedRowEndValue)
{
    var gridItem = document.createElement("div");
    var gridElement = document.getElementsByClassName("grid")[0];
    gridElement.appendChild(gridItem);
    gridItem.style.gridColumnEnd = columnEndValue;
    gridItem.style.gridRowEnd = rowEndValue;

    if (expectedColumnEndValue === undefined)
        expectedColumnEndValue = columnEndValue;
    if (expectedRowEndValue === undefined)
        expectedRowEndValue = rowEndValue;

    var expectedColumnValue = expectedColumnEndValue == "auto" ? expectedColumnEndValue : "auto / " + expectedColumnEndValue;
    var expectedRowValue = expectedRowEndValue == "auto" ? expectedRowEndValue : "auto / " + expectedRowEndValue;

    checkColumnRowValues(gridItem, expectedColumnValue, expectedRowValue);

    gridElement.removeChild(gridItem);
}

window.testColumnRowInvalidJSParsing = function(columnValue, rowValue)
{
    var gridItem = document.createElement("div");
    document.body.appendChild(gridItem);
    gridItem.style.gridColumn = columnValue;
    gridItem.style.gridRow = rowValue;

    checkColumnRowValues(gridItem, "auto", "auto");

    document.body.removeChild(gridItem);
}

var placeholderParentColumnStartValueForInherit = "6";
var placeholderParentColumnEndValueForInherit = "span 2";
var placeholderParentColumnValueForInherit = placeholderParentColumnStartValueForInherit + " / " + placeholderParentColumnEndValueForInherit;
var placeholderParentRowStartValueForInherit = "span 1";
var placeholderParentRowEndValueForInherit = "7";
var placeholderParentRowValueForInherit = placeholderParentRowStartValueForInherit + " / " + placeholderParentRowEndValueForInherit;

var placeholderColumnStartValueForInitial = "1";
var placeholderColumnEndValueForInitial = "span 2";
var placeholderColumnValueForInitial = placeholderColumnStartValueForInitial + " / " + placeholderColumnEndValueForInitial;
var placeholderRowStartValueForInitial = "span 3";
var placeholderRowEndValueForInitial = "5";
var placeholderRowValueForInitial = placeholderRowStartValueForInitial + " / " + placeholderRowEndValueForInitial;

function setupInheritTest()
{
    var parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style.gridColumn = placeholderParentColumnValueForInherit;
    parentElement.style.gridRow = placeholderParentRowValueForInherit;

    var gridItem = document.createElement("div");
    parentElement.appendChild(gridItem);
    return parentElement;
}

function setupInitialTest()
{
    var gridItem = document.createElement("div");
    document.body.appendChild(gridItem);
    gridItem.style.gridColumn = placeholderColumnValueForInitial;
    gridItem.style.gridRow = placeholderRowValueForInitial;

    checkColumnRowValues(gridItem, placeholderColumnValueForInitial, placeholderRowValueForInitial);
    return gridItem;
}

window.testColumnRowInheritJSParsing = function(columnValue, rowValue)
{
    var parentElement = setupInheritTest();
    var gridItem = parentElement.firstChild;
    gridItem.style.gridColumn = columnValue;
    gridItem.style.gridRow = rowValue;

    checkColumnRowValues(gridItem, columnValue !== "inherit" ? columnValue : placeholderParentColumnValueForInherit, rowValue !== "inherit" ? rowValue : placeholderParentRowValueForInherit);

    document.body.removeChild(parentElement);
}

window.testColumnStartRowStartInheritJSParsing = function(columnStartValue, rowStartValue)
{
    var parentElement = setupInheritTest();
    var gridItem = parentElement.firstChild;
    gridItem.style.gridColumnStart = columnStartValue;
    gridItem.style.gridRowStart = rowStartValue;

    // Initial value is 'auto' but we shouldn't touch the opposite grid line.
    var columnValueForInherit = (columnStartValue !== "inherit" ? columnStartValue : placeholderParentColumnStartValueForInherit);
    var rowValueForInherit = (rowStartValue !== "inherit" ? rowStartValue : placeholderParentRowStartValueForInherit);
    checkColumnRowValues(parentElement.firstChild, columnValueForInherit, rowValueForInherit);

    document.body.removeChild(parentElement);
}

window.testColumnEndRowEndInheritJSParsing = function(columnEndValue, rowEndValue)
{
    var parentElement = setupInheritTest();
    var gridItem = parentElement.firstChild;
    gridItem.style.gridColumnEnd = columnEndValue;
    gridItem.style.gridRowEnd = rowEndValue;

    // Initial value is 'auto' but we shouldn't touch the opposite grid line.
    var columnValueForInherit = "auto / " + (columnEndValue !== "inherit" ? columnEndValue : placeholderParentColumnEndValueForInherit);
    var rowValueForInherit = "auto / " + (rowEndValue !== "inherit" ? rowEndValue : placeholderParentRowEndValueForInherit);
    checkColumnRowValues(parentElement.firstChild, columnValueForInherit, rowValueForInherit);

    document.body.removeChild(parentElement);
}

window.testColumnRowInitialJSParsing = function()
{
    var gridItem = setupInitialTest();

    gridItem.style.gridColumn = "initial";
    checkColumnRowValues(gridItem, "auto", placeholderRowValueForInitial);

    gridItem.style.gridRow = "initial";
    checkColumnRowValues(gridItem, "auto", "auto");

    document.body.removeChild(gridItem);
}

window.testColumnStartRowStartInitialJSParsing = function()
{
    var gridItem = setupInitialTest();

    gridItem.style.gridColumnStart = "initial";
    checkColumnRowValues(gridItem, "auto / " + placeholderColumnEndValueForInitial, placeholderRowValueForInitial);

    gridItem.style.gridRowStart = "initial";
    checkColumnRowValues(gridItem,  "auto / " + placeholderColumnEndValueForInitial, "auto / " + placeholderRowEndValueForInitial);

    document.body.removeChild(gridItem);
}

window.testEndAfterInitialJSParsing = function()
{
    var gridItem = setupInitialTest();

    gridItem.style.gridColumnEnd = "initial";
    checkColumnRowValues(gridItem, placeholderColumnStartValueForInitial, placeholderRowValueForInitial);

    gridItem.style.gridRowEnd = "initial";
    checkColumnRowValues(gridItem, placeholderColumnStartValueForInitial, placeholderRowStartValueForInitial);

    document.body.removeChild(gridItem);
}

})();
