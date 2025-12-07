/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

const msPerSecond = 1000;

Utilities.extendObject(window.benchmarkController, {
    updateGraphData: function(testResult, testData, options)
    {
        var element = document.getElementById("test-graph-data");
        element.innerHTML = "";
        element._testResult = testResult;
        element._options = options;

        // top, right, bottom, left.
        var margins = new Insets(30, 30, 50, 40);
        // Note that changes to header content (in onGraphTypeChanged()) can change the available size, so we prepopulate
        // "score" and "confidence" elements with non-breaking spaces in the HTML.
        var size = Point.elementClientSize(element);

        // Convert from compact JSON output to propertied data
        var samplesWithProperties = {};
        [Strings.json.controller, Strings.json.complexity].forEach(function(seriesName) {
            var series = testData[Strings.json.samples][seriesName];
            samplesWithProperties[seriesName] = series.toArray();
        })

        this._targetFrameRate = options["frame-rate"];

        this.createTimeGraph(testResult, samplesWithProperties[Strings.json.controller], testData[Strings.json.marks], testData[Strings.json.controller], options, margins, size);
        this.onTimeGraphOptionsChanged();

        this._showOrHideNodes(true, "form[name=graph-type]");
        document.forms["graph-type"].elements["type"] = "complexity";
        this.createComplexityGraph(testResult, testData[Strings.json.controller], samplesWithProperties, options, margins, size);
        this.onComplexityGraphOptionsChanged();

        this.onGraphTypeChanged();
    },

    _addRegressionLine: function(parent, xScale, yScale, points, range, isAlongYAxis)
    {
        var polygon = [];
        var line = []
        var xRange = isAlongYAxis ? range : 0;
        var yRange = isAlongYAxis ? 0 : range;
        for (var i = 0; i < points.length; ++i) {
            var point = points[i];
            var x;
            if (xRange instanceof Array)
                x = xRange[0];
            else
                x = point[0] + xRange;
            polygon.push(xScale(x), yScale(point[1] + yRange));
            line.push(xScale(point[0]), yScale(point[1]));
        }
        for (var i = points.length - 1; i >= 0; --i) {
            var point = points[i];
            var x;
            if (xRange instanceof Array)
                x = xRange[1];
            else
                x = point[0] - xRange;
            polygon.push(xScale(x), yScale(point[1] - yRange));
        }
        parent.append("polygon")
            .attr("points", polygon.join(","));
        parent.append("line")
            .attr("x1", line[0])
            .attr("y1", line[1])
            .attr("x2", line[2])
            .attr("y2", line[3]);
    },

    _addRegression: function(data, svg, xScale, yScale)
    {
        svg.append("circle")
            .attr("cx", xScale(data.segment1[1][0]))
            .attr("cy", yScale(data.segment1[1][1]))
            .attr("r", 3);
        this._addRegressionLine(svg, xScale, yScale, data.segment1, data.stdev);
        this._addRegressionLine(svg, xScale, yScale, data.segment2, data.stdev);
    },
    
    _tickValuesForFrameRate: function(frameRate, minValue, maxValue)
    {
        // Tick labels go up to 1.5x frame rate
        const buildInFrameRates = {
            15 : [5, 10, 15, 20],
            30 : [5, 10, 15, 20, 25, 30, 35, 40],
            45 : [30, 35, 40, 45, 50, 55, 60],
            60 : [30, 35, 40, 45, 50, 55, 60, 90],
            90 : [30, 35, 40, 45, 50, 55, 60, 90, 120],
            120 : [30, 40, 50, 60, 70, 80, 100, 120, 150],
            144 : [40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 200],
        };
        
        let tickValues = buildInFrameRates[frameRate];
        if (!tickValues) {
            const minLabel = Math.round(minValue / 10) * 10;
            const maxLabel = Math.round(maxValue / 10) * 10;
            tickValues = [];
            let curValue = minLabel;
            while (curValue <= maxLabel) {
                tickValues.push(curValue);
                curValue += 20;
            }
        }
        
        tickValues = tickValues.map((x) => msPerSecond / x);
        return tickValues;
    },
    
    _minFrameRate: function()
    {
        return this._targetFrameRate / 4;
    },

    _maxFrameRate: function()
    {
        return this._targetFrameRate * 1.5;
    },

    createComplexityGraph: function(result, timeRegressions, data, options, margins, size)
    {
        var svg = d3.select("#test-graph-data").append("svg")
            .attr("id", "complexity-graph")
            .attr("class", "hidden")
            .attr("width", size.width)
            .attr("height", size.height)
            .append("g")
                .attr("transform", "translate(" + margins.left + "," + margins.top + ")");

        var timeSamples = data[Strings.json.controller];

        var xMin = 100000, xMax = 0;
        if (timeRegressions) {
            timeRegressions.forEach(function(regression) {
                for (var i = regression.startIndex; i <= regression.endIndex; ++i) {
                    xMin = Math.min(xMin, timeSamples[i].complexity);
                    xMax = Math.max(xMax, timeSamples[i].complexity);
                }
            });
        } else {
            xMin = d3.min(timeSamples, function(s) { return s.complexity; });
            xMax = d3.max(timeSamples, function(s) { return s.complexity; });
        }

        const axisWidth = size.width - margins.left - margins.right;
        const axisHeight = size.height - margins.top - margins.bottom;

        // The y axis is frameLength in ms, inverted with the axis labels showing fps.
        const minFrameRate = this._minFrameRate();
        const maxFrameRate = this._maxFrameRate();

        const yMin = msPerSecond / minFrameRate;
        const yMax = msPerSecond / maxFrameRate;

        var xScale = d3.scale.linear()
            .range([0, axisWidth])
            .domain([xMin, xMax]);
        var yScale = d3.scale.linear()
            .range([axisHeight, 0])
            .domain([yMin, yMax]);

        var xAxis = d3.svg.axis()
            .scale(xScale)
            .orient("bottom");
        var yAxis = d3.svg.axis()
            .scale(yScale)
            .tickValues(this._tickValuesForFrameRate(this._targetFrameRate, minFrameRate, maxFrameRate))
            .tickFormat(function(d) { return (msPerSecond / d).toFixed(0); })
            .orient("left");

        // x-axis
        svg.append("g")
            .attr("class", "x axis")
            .attr("transform", "translate(0," + axisHeight + ")")
            .call(xAxis);

        // y-axis
        var yAxisGroup = svg.append("g")
            .attr("class", "y axis")
            .call(yAxis);

        // time result
        var mean = svg.append("g")
            .attr("class", "mean complexity");
        var timeResult = result[Strings.json.controller];
        this._addRegressionLine(mean, xScale, yScale, [[timeResult.average, yMin], [timeResult.average, yMax]], timeResult.stdev, true);

        // regression
        this._addRegression(result[Strings.json.complexity], svg.append("g").attr("class", "regression raw"), xScale, yScale);

        var bootstrapResult = result[Strings.json.complexity][Strings.json.bootstrap];
        if (bootstrapResult) {
            var histogram = d3.layout.histogram()
                .bins(xScale.ticks(100))(bootstrapResult.data);
            var yBootstrapScale = d3.scale.linear()
                .range([axisHeight/2, 0])
                .domain([0, d3.max(histogram, function(d) { return d.y; })]);
            group = svg.append("g").attr("class", "bootstrap");
            var bar = group.selectAll(".bar")
                .data(histogram)
                .enter().append("g")
                    .attr("class", "bar")
                    .attr("transform", function(d) { return "translate(" + xScale(d.x) + "," + yBootstrapScale(d.y) + ")"; });
            bar.append("rect")
                .attr("x", 1)
                .attr("y", axisHeight/2)
                .attr("width", xScale(histogram[1].x) - xScale(histogram[0].x) - 1)
                .attr("height", function(d) { return axisHeight/2 - yBootstrapScale(d.y); });
            group = group.append("g").attr("class", "median");
            this._addRegressionLine(group, xScale, yScale, [[bootstrapResult.median, yMin], [bootstrapResult.median, yMax]], [bootstrapResult.confidenceLow, bootstrapResult.confidenceHigh], true);
            group.append("circle")
                .attr("cx", xScale(bootstrapResult.median))
                .attr("cy", yScale(msPerSecond / this._targetFrameRate))
                .attr("r", 5);
        }

        // series
        group = svg.append("g")
            .attr("class", "series raw")
            .selectAll("line")
                .data(data[Strings.json.complexity])
                .enter();

        group.append("line")
            .attr("x1", function(d) { return xScale(d.complexity) - 3; })
            .attr("x2", function(d) { return xScale(d.complexity) + 3; })
            .attr("y1", function(d) { return yScale(d.frameLength) - 3; })
            .attr("y2", function(d) { return yScale(d.frameLength) + 3; })
            .attr("class", function(d) { return d.frameType === "m" ? 'mutation' : 'animation'; });
        group.append("line") 
            .attr("x1", function(d) { return xScale(d.complexity) - 3; })
            .attr("x2", function(d) { return xScale(d.complexity) + 3; })
            .attr("y1", function(d) { return yScale(d.frameLength) + 3; })
            .attr("y2", function(d) { return yScale(d.frameLength) - 3; })
            .attr("class", function(d) { return d.frameType === "m" ? 'mutation' : 'animation'; });

        // Cursor
        var cursorGroup = svg.append("g").attr("class", "cursor hidden");
        cursorGroup.append("line")
            .attr("class", "x")
            .attr("x1", 0)
            .attr("x2", 0)
            .attr("y1", yScale(yAxis.scale().domain()[0]) + 10)
            .attr("y2", yScale(yAxis.scale().domain()[1]));
        cursorGroup.append("line")
            .attr("class", "y")
            .attr("x1", xScale(xAxis.scale().domain()[0]) - 10)
            .attr("x2", xScale(xAxis.scale().domain()[1]))
            .attr("y1", 0)
            .attr("y2", 0)
        cursorGroup.append("text")
            .attr("class", "label x")
            .attr("x", 0)
            .attr("y", yScale(yAxis.scale().domain()[0]) + 15)
            .attr("baseline-shift", "-100%")
            .attr("text-anchor", "middle");
        cursorGroup.append("text")
            .attr("class", "label y")
            .attr("x", xScale(xAxis.scale().domain()[0]) - 15)
            .attr("y", 0)
            .attr("baseline-shift", "-30%")
            .attr("text-anchor", "end");
        // Area to handle mouse events
        var area = svg.append("rect")
            .attr("fill", "transparent")
            .attr("x", 0)
            .attr("y", 0)
            .attr("width", size.width)
            .attr("height", axisHeight);

        area.on("mouseover", function() {
            document.querySelector("#complexity-graph .cursor").classList.remove("hidden");
        }).on("mouseout", function() {
            document.querySelector("#complexity-graph .cursor").classList.add("hidden");
        }).on("mousemove", function() {
            var location = d3.mouse(this);
            var location_domain = [xScale.invert(location[0]), yScale.invert(location[1])];
            cursorGroup.select("line.x")
                .attr("x1", location[0])
                .attr("x2", location[0]);
            cursorGroup.select("text.x")
                .attr("x", location[0])
                .text(location_domain[0].toFixed(1));
            cursorGroup.select("line.y")
                .attr("y1", location[1])
                .attr("y2", location[1]);
            cursorGroup.select("text.y")
                .attr("y", location[1])
                .text((msPerSecond / location_domain[1]).toFixed(1));
        });
    },

    createTimeGraph: function(result, samples, marks, regressions, options, margins, size)
    {
        const axisWidth = size.width - margins.left - margins.right;
        const axisHeight = size.height - margins.top - margins.bottom;

        var svg = d3.select("#test-graph-data").append("svg")
            .attr("id", "time-graph")
            .attr("width", size.width)
            .attr("height", size.height)
            .append("g")
                .attr("transform", "translate(" + margins.left + "," + margins.top + ")");

        // Axis scales
        var x = d3.scale.linear()
                .range([0, axisWidth])
                .domain([
                    Math.min(d3.min(samples, function(s) { return s.time; }), 0),
                    d3.max(samples, function(s) { return s.time; })]);
        var complexityMax = d3.max(samples, function(s) {
            if (s.time > 0)
                return s.complexity;
            return 0;
        });
        complexityMax *= 1.2;

        const graphTop = 10;
        var yLeft = d3.scale.linear()
                .range([axisHeight, graphTop])
                .domain([0, complexityMax]);


        const minFrameRate = this._minFrameRate();
        const maxFrameRate = this._maxFrameRate();

        const yRightMin = msPerSecond / minFrameRate;
        const yRightMax = msPerSecond / maxFrameRate;

        var yRight = d3.scale.linear()
                .range([axisHeight, graphTop])
                .domain([yRightMin, yRightMax]);

        // Axes
        var xAxis = d3.svg.axis()
                .scale(x)
                .orient("bottom")
                .tickFormat(function(d) { return (d / msPerSecond).toFixed(0); });
        var yAxisLeft = d3.svg.axis()
                .scale(yLeft)
                .orient("left");

        var yAxisRight = d3.svg.axis()
                .scale(yRight)
                .tickValues(this._tickValuesForFrameRate(this._targetFrameRate, minFrameRate, maxFrameRate))
                .tickFormat(function(d) { return (msPerSecond / d).toFixed(0); })
                .orient("right");

        // x-axis
        svg.append("g")
            .attr("class", "x axis")
            .attr("fill", "rgb(235, 235, 235)")
            .attr("transform", "translate(0," + axisHeight + ")")
            .call(xAxis)
            .append("text")
                .attr("class", "label")
                .attr("x", axisWidth)
                .attr("y", -6)
                .attr("fill", "rgb(235, 235, 235)")
                .style("text-anchor", "end")
                .text("time");

        // yLeft-axis
        svg.append("g")
            .attr("class", "yLeft axis")
            .attr("fill", "#7ADD49")
            .call(yAxisLeft)
            .append("text")
                .attr("class", "label")
                .attr("transform", "rotate(-90)")
                .attr("y", 6)
                .attr("fill", "#7ADD49")
                .attr("dy", ".71em")
                .style("text-anchor", "end")
                .text(Strings.text.complexity);

        // yRight-axis
        svg.append("g")
            .attr("class", "yRight axis")
            .attr("fill", "#FA4925")
            .attr("transform", "translate(" + axisWidth + ", 0)")
            .call(yAxisRight)
            .append("text")
                .attr("class", "label")
                .attr("x", 9)
                .attr("y", -20)
                .attr("fill", "#FA4925")
                .attr("dy", ".71em")
                .style("text-anchor", "start")
                .text(Strings.text.frameRate);

        // marks
        var yMin = yRight(yAxisRight.scale().domain()[0]);
        var yMax = yRight(yAxisRight.scale().domain()[1]);
        for (var markName in marks) {
            var mark = marks[markName];
            var xLocation = x(mark.time);

            var markerGroup = svg.append("g")
                .attr("class", "marker")
                .attr("transform", "translate(" + xLocation + ", 0)");
            markerGroup.append("text")
                .attr("transform", "translate(10, " + (yMin - 10) + ") rotate(-90)")
                .style("text-anchor", "start")
                .text(markName)
            markerGroup.append("line")
                .attr("x1", 0)
                .attr("x2", 0)
                .attr("y1", yMin)
                .attr("y2", yMax);
        }

        if (Strings.json.controller in result) {
            var complexity = result[Strings.json.controller];
            var regression = svg.append("g")
                .attr("class", "complexity mean");
            this._addRegressionLine(regression, x, yLeft, [[samples[0].time, complexity.average], [samples[samples.length - 1].time, complexity.average]], complexity.stdev);
        }
        if (Strings.json.frameLength in result) {
            var frameLength = result[Strings.json.frameLength];
            var regression = svg.append("g")
                .attr("class", "fps mean");
            this._addRegressionLine(regression, x, yRight, [[samples[0].time, msPerSecond / frameLength.average], [samples[samples.length - 1].time, msPerSecond / frameLength.average]], frameLength.stdev);
        }

        // right-target
        if (options["controller"] == "adaptive") {
            var targetFrameLength = msPerSecond / options["frame-rate"];
            svg.append("line")
                .attr("x1", x(0))
                .attr("x2", axisWidth)
                .attr("y1", yRight(targetFrameLength))
                .attr("y2", yRight(targetFrameLength))
                .attr("class", "target-fps marker");
        }

        // Cursor
        var cursorGroup = svg.append("g").attr("class", "cursor");
        cursorGroup.append("line")
            .attr("x1", 0)
            .attr("x2", 0)
            .attr("y1", yMin)
            .attr("y2", yMin);

        // Data
        var allData = samples;
        var filteredData = samples.filter(function (sample) {
            return "smoothedFrameLength" in sample;
        });

        function addData(name, data, yCoordinateCallback, pointRadius, omitLine) {
            var svgGroup = svg.append("g").attr("id", name);
            if (!omitLine) {
                svgGroup.append("path")
                    .datum(data)
                    .attr("d", d3.svg.line()
                        .x(function(d) { return x(d.time); })
                        .y(yCoordinateCallback));
            }
            svgGroup.selectAll("circle")
                .data(data)
                .enter()
                .append("circle")
                .attr("cx", function(d) { return x(d.time); })
                .attr("cy", yCoordinateCallback)
                .attr("r", pointRadius);

            cursorGroup.append("circle")
                .attr("class", name)
                .attr("r", pointRadius + 2);
        }

        addData("complexity", allData, function(d) { return yLeft(d.complexity); }, 2);
        addData("rawFPS", allData, function(d) { return yRight(d.frameLength); }, 1);
        addData("filteredFPS", filteredData, function(d) { return yRight(d.smoothedFrameLength); }, 2);

        // regressions
        var regressionGroup = svg.append("g")
            .attr("id", "regressions");
        if (regressions) {
            var complexities = [];
            regressions.forEach(function (regression) {
                if (!isNaN(regression.segment1[0][1]) && !isNaN(regression.segment1[1][1])) {
                    regressionGroup.append("line")
                        .attr("x1", x(regression.segment1[0][0]))
                        .attr("x2", x(regression.segment1[1][0]))
                        .attr("y1", yRight(regression.segment1[0][1]))
                        .attr("y2", yRight(regression.segment1[1][1]));
                }
                if (!isNaN(regression.segment2[0][1]) && !isNaN(regression.segment2[1][1])) {
                    regressionGroup.append("line")
                        .attr("x1", x(regression.segment2[0][0]))
                        .attr("x2", x(regression.segment2[1][0]))
                        .attr("y1", yRight(regression.segment2[0][1]))
                        .attr("y2", yRight(regression.segment2[1][1]));
                }
                // inflection point
                regressionGroup.append("circle")
                    .attr("cx", x(regression.segment2[0][0]))
                    .attr("cy", yRight(regression.segment2[0][1]))
                    .attr("r", 3);
                regressionGroup.append("line")
                    .attr("class", "association")
                    .attr("stroke-dasharray", "5, 3")
                    .attr("x1", x(regression.segment2[0][0]))
                    .attr("x2", x(regression.segment2[0][0]))
                    .attr("y1", yRight(regression.segment2[0][1]))
                    .attr("y2", yLeft(regression.complexity));
                regressionGroup.append("circle")
                    .attr("cx", x(regression.segment1[1][0]))
                    .attr("cy", yLeft(regression.complexity))
                    .attr("r", 5);
                complexities.push(regression.complexity);
            });
            if (complexities.length) {
                var yLeftComplexities = d3.svg.axis()
                    .scale(yLeft)
                    .tickValues(complexities)
                    .tickSize(10)
                    .orient("left");
                svg.append("g")
                    .attr("class", "complexity yLeft axis")
                    .call(yLeftComplexities);
            }
        }

        // Area to handle mouse events
        var area = svg.append("rect")
            .attr("fill", "transparent")
            .attr("x", 0)
            .attr("y", 0)
            .attr("width", axisWidth)
            .attr("height", axisHeight);

        var timeBisect = d3.bisector(function(d) { return d.time; }).right;
        var statsToHighlight = ["complexity", "rawFPS", "filteredFPS"];
        area.on("mouseover", function() {
            document.querySelector("#time-graph .cursor").classList.remove("hidden");
            document.querySelector("#test-graph nav").classList.remove("hide-data");
        }).on("mouseout", function() {
            document.querySelector("#time-graph .cursor").classList.add("hidden");
            document.querySelector("#test-graph nav").classList.add("hide-data");
        }).on("mousemove", function() {
            var form = document.forms["time-graph-options"].elements;

            var mx_domain = x.invert(d3.mouse(this)[0]);
            var index = Math.min(timeBisect(allData, mx_domain), allData.length - 1);
            var data = allData[index];
            var cursor_x = x(data.time);
            var cursor_y = yAxisRight.scale().domain()[1];
            var ys = [yRight(yAxisRight.scale().domain()[0]), yRight(yAxisRight.scale().domain()[1])];

            document.querySelector("#test-graph nav .time").textContent = (data.time / msPerSecond).toFixed(4) + "s (" + index + ")";
            statsToHighlight.forEach(function(name) {
                var element = document.querySelector("#test-graph nav ." + name);
                var content = "";
                var data_y = null;
                switch (name) {
                case "complexity":
                    content = data.complexity;
                    data_y = yLeft(data.complexity);
                    break;
                case "rawFPS":
                    content = (msPerSecond / data.frameLength).toFixed(2);
                    data_y = yRight(data.frameLength);
                    break;
                case "filteredFPS":
                    if ("smoothedFrameLength" in data) {
                        content = (msPerSecond / data.smoothedFrameLength).toFixed(2);
                        data_y = yRight(data.smoothedFrameLength);
                    }
                    break;
                }

                element.textContent = content;

                if (form[name].checked && data_y !== null) {
                    ys.push(data_y);
                    cursorGroup.select("." + name)
                        .attr("cx", cursor_x)
                        .attr("cy", data_y);
                    document.querySelector("#time-graph .cursor ." + name).classList.remove("hidden");
                } else
                    document.querySelector("#time-graph .cursor ." + name).classList.add("hidden");
            });

            if (form["rawFPS"].checked)
                cursor_y = Math.max(cursor_y, data.frameLength);
            cursorGroup.select("line")
                .attr("x1", cursor_x)
                .attr("x2", cursor_x)
                .attr("y1", Math.min.apply(null, ys))
                .attr("y2", Math.max.apply(null, ys));

        });
    },

    _showOrHideNodes: function(isShown, selector) {
        var nodeList = document.querySelectorAll(selector);
        if (isShown) {
            for (var i = 0; i < nodeList.length; ++i)
                nodeList[i].classList.remove("hidden");
        } else {
            for (var i = 0; i < nodeList.length; ++i)
                nodeList[i].classList.add("hidden");
        }
    },

    onComplexityGraphOptionsChanged: function() {
        var form = document.forms["complexity-graph-options"].elements;
        benchmarkController._showOrHideNodes(form["series-raw"].checked, "#complexity-graph .series.raw");
        benchmarkController._showOrHideNodes(form["regression-time-score"].checked, "#complexity-graph .mean.complexity");
        benchmarkController._showOrHideNodes(form["bootstrap-score"].checked, "#complexity-graph .bootstrap");
        benchmarkController._showOrHideNodes(form["complexity-regression-aggregate-raw"].checked, "#complexity-graph .regression.raw");
    },

    onTimeGraphOptionsChanged: function() {
        var form = document.forms["time-graph-options"].elements;
        benchmarkController._showOrHideNodes(form["markers"].checked, ".marker");
        benchmarkController._showOrHideNodes(form["averages"].checked, "#test-graph-data .mean");
        benchmarkController._showOrHideNodes(form["complexity"].checked, "#complexity");
        benchmarkController._showOrHideNodes(form["rawFPS"].checked, "#rawFPS");
        benchmarkController._showOrHideNodes(form["filteredFPS"].checked, "#filteredFPS");
        benchmarkController._showOrHideNodes(form["regressions"].checked, "#regressions");
    },

    onGraphTypeChanged: function() {
        var form = document.forms["graph-type"].elements;
        var testResult = document.getElementById("test-graph-data")._testResult;
        var isTimeSelected = form["graph-type"].value == "time";

        benchmarkController._showOrHideNodes(isTimeSelected, "#time-graph");
        benchmarkController._showOrHideNodes(isTimeSelected, "form[name=time-graph-options]");
        benchmarkController._showOrHideNodes(!isTimeSelected, "#complexity-graph");
        benchmarkController._showOrHideNodes(!isTimeSelected, "form[name=complexity-graph-options]");

        var score = "", mean = "";
        if (isTimeSelected) {
            score = testResult[Strings.json.score].toFixed(2);

            var regression = testResult[Strings.json.controller];
            mean = [
                "mean: ",
                regression.average.toFixed(2),
                " ± ",
                regression.stdev.toFixed(2),
                " (",
                regression.percent.toFixed(2),
                "%)"];
            if (regression.concern) {
                mean = mean.concat([
                    ", worst 5%: ",
                    regression.concern.toFixed(2)]);
            }
            mean = mean.join("");
        } else {
            var complexityRegression = testResult[Strings.json.complexity];
            document.getElementById("complexity-regression-aggregate-raw").textContent = complexityRegression.complexity.toFixed(2) + ", ±" + complexityRegression.stdev.toFixed(2) + "ms";
            var bootstrap = complexityRegression[Strings.json.bootstrap];
            if (bootstrap) {
                score = bootstrap.median.toFixed(2);
                mean = [
                    (bootstrap.confidencePercentage * 100).toFixed(0),
                    "% CI: ",
                    bootstrap.confidenceLow.toFixed(2),
                    "–",
                    bootstrap.confidenceHigh.toFixed(2)
                ].join("");
            }
        }

        sectionsManager.setSectionScore("test-graph", score, mean, this._targetFrameRate);
    }
});
