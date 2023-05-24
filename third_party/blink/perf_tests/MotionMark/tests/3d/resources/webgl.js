/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
(function() {

WebGLStage = Utilities.createSubclass(Stage,
    function(element, options)
    {
        Stage.call(this);
    },
    {

        initialize: function(benchmark, options)
        {
            Stage.prototype.initialize.call(this, benchmark, options);

            this._numTriangles = 0;
            this._bufferSize = 0;

            this._gl = this.element.getContext("webgl");
            var gl = this._gl;

            gl.clearColor(0, 0, 0, 1);

            // Create the vertex shader object.
            var vertexShader = gl.createShader(gl.VERTEX_SHADER);

            // The source code for the shader is extracted from the <script> element above.
            gl.shaderSource(vertexShader, this._getFunctionSource("vertex"));

            // Compile the shader.
            gl.compileShader(vertexShader);
            if (!gl.getShaderParameter(vertexShader, gl.COMPILE_STATUS)) {
                // We failed to compile. Output to the console and quit.
                console.error("Vertex Shader failed to compile.");
                console.error(gl.getShaderInfoLog(vertexShader));
                return;
            }

            // Now do the fragment shader.
            var fragmentShader = gl.createShader(gl.FRAGMENT_SHADER);
            gl.shaderSource(fragmentShader, this._getFunctionSource("fragment"));
            gl.compileShader(fragmentShader);
            if (!gl.getShaderParameter(fragmentShader, gl.COMPILE_STATUS)) {
                console.error("Fragment Shader failed to compile.");
                console.error(gl.getShaderInfoLog(fragmentShader));
                return;
            }

            // We have two compiled shaders. Time to make the program.
            var program = gl.createProgram();
            gl.attachShader(program, vertexShader);
            gl.attachShader(program, fragmentShader);
            gl.linkProgram(program);

            if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
                console.error("Unable to link shaders into program.");
                return;
            }

            // Our program has two inputs. We have a single uniform "color",
            // and one vertex attribute "position".

            gl.useProgram(program);
            this._uScale = gl.getUniformLocation(program, "scale");
            this._uTime = gl.getUniformLocation(program, "time");
            this._uOffsetX = gl.getUniformLocation(program, "offsetX");
            this._uOffsetY = gl.getUniformLocation(program, "offsetY");
            this._uScalar = gl.getUniformLocation(program, "scalar");
            this._uScalarOffset = gl.getUniformLocation(program, "scalarOffset");

            this._aPosition = gl.getAttribLocation(program, "position");
            gl.enableVertexAttribArray(this._aPosition);

            this._aColor = gl.getAttribLocation(program, "color");
            gl.enableVertexAttribArray(this._aColor);

            this._positionData = new Float32Array([
                // x y z 1
                   0,  0.1, 0, 1,
                -0.1, -0.1, 0, 1,
                 0.1, -0.1, 0, 1
            ]);
            this._positionBuffer = gl.createBuffer();
            gl.bindBuffer(gl.ARRAY_BUFFER, this._positionBuffer);
            gl.bufferData(gl.ARRAY_BUFFER, this._positionData, gl.STATIC_DRAW);

            this._colorData = new Float32Array([
                1, 0, 0, 1,
                0, 1, 0, 1,
                0, 0, 1, 1
            ]);
            this._colorBuffer = gl.createBuffer();
            gl.bindBuffer(gl.ARRAY_BUFFER, this._colorBuffer);
            gl.bufferData(gl.ARRAY_BUFFER, this._colorData, gl.STATIC_DRAW);

            this._resetIfNecessary();
        },

        _getFunctionSource: function(id)
        {
            return document.getElementById(id).text;
        },

        _resetIfNecessary: function()
        {
            if (this._numTriangles <= this._bufferSize)
                return;

            if (!this._bufferSize)
                this._bufferSize = 128;

            while (this._numTriangles > this._bufferSize)
                this._bufferSize *= 4;

            this._uniformData = new Float32Array(this._bufferSize * 6);
            for (var i = 0; i < this._bufferSize; ++i) {
                this._uniformData[i * 6 + 0] = Stage.random(0.2, 0.4);
                this._uniformData[i * 6 + 1] = 0;
                this._uniformData[i * 6 + 2] = Stage.random(-0.9, 0.9);
                this._uniformData[i * 6 + 3] = Stage.random(-0.9, 0.9);
                this._uniformData[i * 6 + 4] = Stage.random(0.5, 2);
                this._uniformData[i * 6 + 5] = Stage.random(0, 10);
            }
        },

        tune: function(count)
        {
            if (!count)
                return;

            this._numTriangles += count;
            this._numTriangles = Math.max(this._numTriangles, 0);

            this._resetIfNecessary();
        },

        animate: function(timeDelta)
        {
            var gl = this._gl;

            gl.clear(gl.COLOR_BUFFER_BIT);

            if (!this._startTime)
                this._startTime = Stage.dateCounterValue(1000);
            var elapsedTime = Stage.dateCounterValue(1000) - this._startTime;

            for (var i = 0; i < this._numTriangles; ++i) {

                this._uniformData[i * 6 + 1] = elapsedTime;

                var uniformDataOffset = i * 6;
                gl.uniform1f(this._uScale, this._uniformData[uniformDataOffset++]);
                gl.uniform1f(this._uTime, this._uniformData[uniformDataOffset++]);
                gl.uniform1f(this._uOffsetX, this._uniformData[uniformDataOffset++]);
                gl.uniform1f(this._uOffsetY, this._uniformData[uniformDataOffset++]);
                gl.uniform1f(this._uScalar, this._uniformData[uniformDataOffset++]);
                gl.uniform1f(this._uScalarOffset, this._uniformData[uniformDataOffset++]);

                gl.bindBuffer(gl.ARRAY_BUFFER, this._positionBuffer);
                gl.vertexAttribPointer(this._aPosition, 4, gl.FLOAT, false, 0, 0);

                gl.bindBuffer(gl.ARRAY_BUFFER, this._colorBuffer);
                gl.vertexAttribPointer(this._aColor, 4, gl.FLOAT, false, 0, 0);

                gl.drawArrays(gl.TRIANGLES, 0, 3);
            }

        },

        complexity: function()
        {
            return this._numTriangles;
        }
    }
);

WebGLBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new WebGLStage(), options);
    }
);

window.benchmarkClass = WebGLBenchmark;

})();
