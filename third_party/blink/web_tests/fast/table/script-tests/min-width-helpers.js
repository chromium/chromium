var testNotes = "======== General notes ========\n\
\n\
The stylesheet used to style the table in each test is available at: <a href=\"resources/min-width.css\">LayoutTests/fast/table/resources/min-width.css</a>\n\
\n\
Most importantly, note that each table has:\n\
- minimum intrinsic width and height both equal to 100px based on the table content\n\
- maximum intrinsic width and height both equal to 250px based on the table content\n\
- borders and paddings that add up to 30px in both the horizontal and vertical directions\n\
- a parent whose dimensions are 1000px by 1000px\n\
\n\
The function signature of computeLogicalWidth is:\n\
function computeLogicalWidth(writingMode, direction, tableStyle)\n";

/* All tables will be generated to have the following intrinsic widths. */
var minIntrinsicLogicalWidth = 100;
var maxIntrinsicLogicalWidth = 250;

/* Tests will cover all permutations of the follow properties and settings. */
var tableTypes = ["html", "css"];
var displays = ["block", "inline"]
var writingModes = ["horizontal", "vertical"];
var directions = ["ltr", "rtl"];
var logicalWidthsCombinations = [
    /* fixed min-width, auto width */
    {"min-width": "500px", "width": null, "computed-width": {"css": "500px", "html": "500px"}},
    {"min-width": "150px", "width": null, "computed-width": {"css": "250px", "html": "280px"}},
    {"min-width": "50px", "width": null, "computed-width": {"css": "250px", "html": "280px"}},
    /* fixed min-width, fixed width */
    {"min-width": "500px", "width": "600px", "computed-width": {"css": "600px", "html": "600px"}},
    {"min-width": "500px", "width": "400px", "computed-width": {"css": "500px", "html": "500px"}},
    /* fixed min-width, percent width */
    {"min-width": "500px", "width": "60%", "computed-width": {"css": "600px", "html": "600px"}},
    {"min-width": "500px", "width": "40%", "computed-width": {"css": "500px", "html": "500px"}},
    /* percent min-width, auto width */
    {"min-width": "50%", "width": null, "computed-width": {"css": "500px", "html": "500px"}},
    {"min-width": "15%", "width": null, "computed-width": {"css": "250px", "html": "280px"}},
    {"min-width": "5%", "width": null, "computed-width": {"css": "250px", "html": "280px"}},
    /* percent min-width, fixed width */
    {"min-width": "50%", "width": "600px", "computed-width": {"css": "600px", "html": "600px"}},
    {"min-width": "50%", "width": "400px", "computed-width": {"css": "500px", "html": "500px"}},
     /* percent min-width, percent width */
    {"min-width": "50%", "width": "60%", "computed-width": {"css": "600px", "html": "600px"}},
    {"min-width": "50%", "width": "40%", "computed-width": {"css": "500px", "html": "500px"}},
     /* auto min-width (shouldn't affect anything), auto width */
    {"min-width": "auto", "width": null, "computed-width": {"css": "250px", "html": "280px"}},
];

function runTests(tableType)
{
    debug(testNotes);

    writingModes.forEach(function(writingMode) {
        debug("======== Test " + writingMode + " writing mode ========\n");

        directions.forEach(function(direction) {
            debug("==== Test " + direction + " direction ====\n");

            logicalWidthsCombinations.forEach(function(logicalWidthsCombination) {
                var tableStyle = createTableStyle(writingMode, logicalWidthsCombination);
                shouldEvaluateTo("computeLogicalWidth('" + writingMode + "', '" + direction + "', '" + tableStyle + "')", "'" + logicalWidthsCombination["computed-width"][tableType] + "'");
            });

            debug("");
        });
    });
}

function createTableStyle(writingMode, logicalWidthsCombination)
{
    var widthStyle = "";

    var logicalWidthName = (writingMode == "vertical" ? "height" : "width");

    if (logicalWidthsCombination["width"] != null)
        widthStyle += logicalWidthName + ": " + logicalWidthsCombination["width"] + "; ";

    if (logicalWidthsCombination["min-width"] != null)
        widthStyle += "min-" + logicalWidthName + ": " + logicalWidthsCombination["min-width"] + ";";

    return widthStyle;
}

function computeLogicalWidthHelper(tableType, display, writingMode, direction, tableStyle)
{
    var isCSSTable = (tableType == "css");
    var tableClass = display + " " + writingMode + " " + direction;

    var tableParent = document.createElement("div");
    tableParent.setAttribute("class", "table-parent");
    document.body.appendChild(tableParent);

    var table = document.createElement(isCSSTable ? "div" : "table");
    table.setAttribute("class", tableClass);
    table.setAttribute("style", tableStyle);
    tableParent.appendChild(table);

    var rowGroup = document.createElement(isCSSTable ? "div" : "tbody");
    rowGroup.setAttribute("class", "row-group");
    table.appendChild(rowGroup);

    var row = document.createElement(isCSSTable ? "div" : "tr");
    row.setAttribute("class", "row");
    rowGroup.appendChild(row);

    var cell = document.createElement(isCSSTable ? "div" : "td");
    cell.setAttribute("class", "cell");
    row.appendChild(cell);

    // Create as many spans of width equal to minIntrinsicLogicalWidth without exceeding maxIntrinsicLogicalWidth.
    var remainingLogicalWidth;
    for (remainingLogicalWidth = maxIntrinsicLogicalWidth; remainingLogicalWidth >= minIntrinsicLogicalWidth; remainingLogicalWidth -= minIntrinsicLogicalWidth) {
        span = createSpan(minIntrinsicLogicalWidth);
        cell.appendChild(span);
    }

    // Create a span of width < minIntrinsicLogicalWidth for any remaining width.
    if (remainingLogicalWidth > 0) {
        span = createSpan(remainingLogicalWidth);
        cell.appendChild(span);
    }

    var logicalWidthPropertyName = (writingMode == "vertical" ? "height" : "width");
    var computedLogicalWidth = window.getComputedStyle(table, null).getPropertyValue(logicalWidthPropertyName);

    document.body.removeChild(tableParent);

    return computedLogicalWidth;
}

function createSpan(logicalWidth)
{
    var span = document.createElement("span");
    span.setAttribute("style", "display: inline-block; width: " + logicalWidth + "px; height: " + logicalWidth + "px; background-color: #f00;");
    return span;
}
