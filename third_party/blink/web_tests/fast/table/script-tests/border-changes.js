description(
"This test checks the behavior of table styles when changing certain table attributes."
);

var yellow = "rgb(255, 255, 0)";
var orange = "rgb(255, 165, 0)";
var red = "rgb(255, 0, 0)";

var table = document.createElement("table");
table.setAttribute("style", "border-color: yellow");
var row = table.insertRow(-1);
row.setAttribute("style", "border-color: orange");
var cell = row.insertCell(-1);
cell.setAttribute("style", "border-color: red");

document.body.appendChild(table);

shouldBe("getComputedStyle(table, '').borderLeftWidth", "'0px'");
shouldBe("getComputedStyle(table, '').borderRightWidth", "'0px'");
shouldBe("getComputedStyle(table, '').borderTopWidth", "'0px'");
shouldBe("getComputedStyle(table, '').borderBottomWidth", "'0px'");
shouldBe("getComputedStyle(table, '').borderLeftStyle", "'none'");
shouldBe("getComputedStyle(table, '').borderRightStyle", "'none'");
shouldBe("getComputedStyle(table, '').borderTopStyle", "'none'");
shouldBe("getComputedStyle(table, '').borderBottomStyle", "'none'");
shouldBe("getComputedStyle(table, '').borderLeftColor", "yellow");
shouldBe("getComputedStyle(table, '').borderRightColor", "yellow");
shouldBe("getComputedStyle(table, '').borderTopColor", "yellow");
shouldBe("getComputedStyle(table, '').borderBottomColor", "yellow");

shouldBe("getComputedStyle(cell, '').borderLeftWidth", "'0px'");
shouldBe("getComputedStyle(cell, '').borderRightWidth", "'0px'");
shouldBe("getComputedStyle(cell, '').borderTopWidth", "'0px'");
shouldBe("getComputedStyle(cell, '').borderBottomWidth", "'0px'");
shouldBe("getComputedStyle(cell, '').borderLeftStyle", "'none'");
shouldBe("getComputedStyle(cell, '').borderRightStyle", "'none'");
shouldBe("getComputedStyle(cell, '').borderTopStyle", "'none'");
shouldBe("getComputedStyle(cell, '').borderBottomStyle", "'none'");
shouldBe("getComputedStyle(cell, '').borderLeftColor", "red");
shouldBe("getComputedStyle(cell, '').borderRightColor", "red");
shouldBe("getComputedStyle(cell, '').borderTopColor", "red");
shouldBe("getComputedStyle(cell, '').borderBottomColor", "red");

table.border = '';

shouldBe("getComputedStyle(table, '').borderLeftWidth", "'1px'");
shouldBe("getComputedStyle(table, '').borderRightWidth", "'1px'");
shouldBe("getComputedStyle(table, '').borderTopWidth", "'1px'");
shouldBe("getComputedStyle(table, '').borderBottomWidth", "'1px'");
shouldBe("getComputedStyle(table, '').borderLeftStyle", "'outset'");
shouldBe("getComputedStyle(table, '').borderRightStyle", "'outset'");
shouldBe("getComputedStyle(table, '').borderTopStyle", "'outset'");
shouldBe("getComputedStyle(table, '').borderBottomStyle", "'outset'");
shouldBe("getComputedStyle(table, '').borderLeftColor", "yellow");
shouldBe("getComputedStyle(table, '').borderRightColor", "yellow");
shouldBe("getComputedStyle(table, '').borderTopColor", "yellow");
shouldBe("getComputedStyle(table, '').borderBottomColor", "yellow");

shouldBe("getComputedStyle(cell, '').borderLeftWidth", "'1px'");
shouldBe("getComputedStyle(cell, '').borderRightWidth", "'1px'");
shouldBe("getComputedStyle(cell, '').borderTopWidth", "'1px'");
shouldBe("getComputedStyle(cell, '').borderBottomWidth", "'1px'");
shouldBe("getComputedStyle(cell, '').borderLeftStyle", "'inset'");
shouldBe("getComputedStyle(cell, '').borderRightStyle", "'inset'");
shouldBe("getComputedStyle(cell, '').borderTopStyle", "'inset'");
shouldBe("getComputedStyle(cell, '').borderBottomStyle", "'inset'");
shouldBe("getComputedStyle(cell, '').borderLeftColor", "red");
shouldBe("getComputedStyle(cell, '').borderRightColor", "red");
shouldBe("getComputedStyle(cell, '').borderTopColor", "red");
shouldBe("getComputedStyle(cell, '').borderBottomColor", "red");

table.setAttribute("bordercolor", "green");

shouldBe("getComputedStyle(table, '').borderLeftWidth", "'1px'");
shouldBe("getComputedStyle(table, '').borderRightWidth", "'1px'");
shouldBe("getComputedStyle(table, '').borderTopWidth", "'1px'");
shouldBe("getComputedStyle(table, '').borderBottomWidth", "'1px'");
shouldBe("getComputedStyle(table, '').borderLeftStyle", "'solid'");
shouldBe("getComputedStyle(table, '').borderRightStyle", "'solid'");
shouldBe("getComputedStyle(table, '').borderTopStyle", "'solid'");
shouldBe("getComputedStyle(table, '').borderBottomStyle", "'solid'");
shouldBe("getComputedStyle(table, '').borderLeftColor", "yellow");
shouldBe("getComputedStyle(table, '').borderRightColor", "yellow");
shouldBe("getComputedStyle(table, '').borderTopColor", "yellow");
shouldBe("getComputedStyle(table, '').borderBottomColor", "yellow");

shouldBe("getComputedStyle(cell, '').borderLeftWidth", "'1px'");
shouldBe("getComputedStyle(cell, '').borderRightWidth", "'1px'");
shouldBe("getComputedStyle(cell, '').borderTopWidth", "'1px'");
shouldBe("getComputedStyle(cell, '').borderBottomWidth", "'1px'");
shouldBe("getComputedStyle(cell, '').borderLeftStyle", "'solid'");
shouldBe("getComputedStyle(cell, '').borderRightStyle", "'solid'");
shouldBe("getComputedStyle(cell, '').borderTopStyle", "'solid'");
shouldBe("getComputedStyle(cell, '').borderBottomStyle", "'solid'");
shouldBe("getComputedStyle(cell, '').borderLeftColor", "red");
shouldBe("getComputedStyle(cell, '').borderRightColor", "red");
shouldBe("getComputedStyle(cell, '').borderTopColor", "red");
shouldBe("getComputedStyle(cell, '').borderBottomColor", "red");

table.rules = "cols";

shouldBe("getComputedStyle(table, '').borderLeftWidth", "'1px'");
shouldBe("getComputedStyle(table, '').borderRightWidth", "'1px'");
shouldBe("getComputedStyle(table, '').borderTopWidth", "'1px'");
shouldBe("getComputedStyle(table, '').borderBottomWidth", "'1px'");
shouldBe("getComputedStyle(table, '').borderLeftStyle", "'solid'");
shouldBe("getComputedStyle(table, '').borderRightStyle", "'solid'");
shouldBe("getComputedStyle(table, '').borderTopStyle", "'solid'");
shouldBe("getComputedStyle(table, '').borderBottomStyle", "'solid'");
shouldBe("getComputedStyle(table, '').borderLeftColor", "yellow");
shouldBe("getComputedStyle(table, '').borderRightColor", "yellow");
shouldBe("getComputedStyle(table, '').borderTopColor", "yellow");
shouldBe("getComputedStyle(table, '').borderBottomColor", "yellow");

shouldBe("getComputedStyle(cell, '').borderLeftWidth", "'1px'");
shouldBe("getComputedStyle(cell, '').borderRightWidth", "'1px'");
shouldBe("getComputedStyle(cell, '').borderTopWidth", "'0px'");
shouldBe("getComputedStyle(cell, '').borderBottomWidth", "'0px'");
shouldBe("getComputedStyle(cell, '').borderLeftStyle", "'solid'");
shouldBe("getComputedStyle(cell, '').borderRightStyle", "'solid'");
shouldBe("getComputedStyle(cell, '').borderTopStyle", "'none'");
shouldBe("getComputedStyle(cell, '').borderBottomStyle", "'none'");
shouldBe("getComputedStyle(cell, '').borderLeftColor", "red");
shouldBe("getComputedStyle(cell, '').borderRightColor", "red");
shouldBe("getComputedStyle(cell, '').borderTopColor", "red");
shouldBe("getComputedStyle(cell, '').borderBottomColor", "red");

table.rules = "rows";

shouldBe("getComputedStyle(table, '').borderLeftWidth", "'1px'");
shouldBe("getComputedStyle(table, '').borderRightWidth", "'1px'");
shouldBe("getComputedStyle(table, '').borderTopWidth", "'1px'");
shouldBe("getComputedStyle(table, '').borderBottomWidth", "'1px'");
shouldBe("getComputedStyle(table, '').borderLeftStyle", "'solid'");
shouldBe("getComputedStyle(table, '').borderRightStyle", "'solid'");
shouldBe("getComputedStyle(table, '').borderTopStyle", "'solid'");
shouldBe("getComputedStyle(table, '').borderBottomStyle", "'solid'");
shouldBe("getComputedStyle(table, '').borderLeftColor", "yellow");
shouldBe("getComputedStyle(table, '').borderRightColor", "yellow");
shouldBe("getComputedStyle(table, '').borderTopColor", "yellow");
shouldBe("getComputedStyle(table, '').borderBottomColor", "yellow");

shouldBe("getComputedStyle(cell, '').borderLeftWidth", "'0px'");
shouldBe("getComputedStyle(cell, '').borderRightWidth", "'0px'");
shouldBe("getComputedStyle(cell, '').borderTopWidth", "'1px'");
shouldBe("getComputedStyle(cell, '').borderBottomWidth", "'1px'");
shouldBe("getComputedStyle(cell, '').borderLeftStyle", "'none'");
shouldBe("getComputedStyle(cell, '').borderRightStyle", "'none'");
shouldBe("getComputedStyle(cell, '').borderTopStyle", "'solid'");
shouldBe("getComputedStyle(cell, '').borderBottomStyle", "'solid'");
shouldBe("getComputedStyle(cell, '').borderLeftColor", "red");
shouldBe("getComputedStyle(cell, '').borderRightColor", "red");
shouldBe("getComputedStyle(cell, '').borderTopColor", "red");
shouldBe("getComputedStyle(cell, '').borderBottomColor", "red");

// bordercolor should not trigger a border if the border attribute is not set.
table.removeAttribute("border");

shouldBe("getComputedStyle(table, '').borderLeftWidth", "'0px'");
shouldBe("getComputedStyle(table, '').borderRightWidth", "'0px'");
shouldBe("getComputedStyle(table, '').borderTopWidth", "'0px'");
shouldBe("getComputedStyle(table, '').borderBottomWidth", "'0px'");
shouldBe("getComputedStyle(table, '').borderLeftStyle", "'hidden'");
shouldBe("getComputedStyle(table, '').borderRightStyle", "'hidden'");
shouldBe("getComputedStyle(table, '').borderTopStyle", "'hidden'");
shouldBe("getComputedStyle(table, '').borderBottomStyle", "'hidden'");
shouldBe("getComputedStyle(table, '').borderLeftColor", "yellow");
shouldBe("getComputedStyle(table, '').borderRightColor", "yellow");
shouldBe("getComputedStyle(table, '').borderTopColor", "yellow");
shouldBe("getComputedStyle(table, '').borderBottomColor", "yellow");

document.body.removeChild(table);
