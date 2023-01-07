function RenderWithCanvas(parent) {
    test_cases.forEach(async function(test_case) {
        var heading = document.createElement("b");
        heading.innerText = test_case.description;
        parent.appendChild(heading);

        var canvas = document.createElement("canvas");
        canvas.setAttribute("width", test_case.width);
        canvas.setAttribute("style", "border: 1px solid black");
        canvas.setAttribute("height", test_case.height ? test_case.height : 60);
        parent.appendChild(canvas);
        var text = FormattedText.format(test_case.text,
          `color:black; font:${test_case.font_size}px Arial`, canvas.width);

        var context = canvas.getContext("2d", { alpha: false });
        context.clearRect(0,0,test_case.width, canvas.height);
        context.fillStyle = "#FFFFFF";
        context.fillRect(0,0,test_case.width, canvas.height);
        var y = (test_case.y === undefined ? 0 : test_case.y);
        context.drawFormattedText(text, 0, y);
    });
}

function RenderWithDiv(parent) {
    test_cases.forEach(async function(test_case) {
        var heading = document.createElement("b");
        heading.innerText = test_case.description;
        parent.appendChild(heading);

        var div = document.createElement("div");
        div.setAttribute("width", test_case.width);
        var style = "border: 1px solid black; overflow:hidden;";
        style += ("width:" + test_case.width + "px;");
        style += ("height:" + (test_case.height ? test_case.height : 60)  + "px;");
        style += ("font: "+ test_case.font_size + "px Arial" + " ");
        div.setAttribute("style", style);
        div.innerText = test_case.text;
        parent.appendChild(div);
    });
}
