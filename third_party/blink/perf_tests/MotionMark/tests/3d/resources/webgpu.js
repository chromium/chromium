/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

const whlslShaders = `
struct VertexOutput {
  float4 position : SV_Position;
  float4 color : attribute(1);
}

vertex VertexOutput vertexMain(float4 position : attribute(0),
                                float4 color : attribute(1),
                                constant float[] timeUniform : register(b0, space0),
                                constant float[] uniforms : register(b0, space1)) {
  float scale = uniforms[0];
  float offsetX = uniforms[1];
  float offsetY = uniforms[2];
  float scalar = uniforms[3];
  float scalarOffset = uniforms[4];
  float time = timeUniform[0];

  float fade = fmod(scalarOffset + time * scalar / 10.0, 1.0);
  if (fade < 0.5) {
      fade = fade * 2.0;
  } else {
      fade = (1.0 - fade) * 2.0;
  }
  float xpos = position.x * scale;
  float ypos = position.y * scale;
  float angle = 3.14159 * 2.0 * fade;
  float xrot = xpos * cos(angle) - ypos * sin(angle);
  float yrot = xpos * sin(angle) + ypos * cos(angle);
  xpos = xrot + offsetX;
  ypos = yrot + offsetY;

  VertexOutput out;
  out.position = float4(xpos, ypos, 0.0, 1.0);
  out.color = float4(fade, 1.0 - fade, 0.0, 1.0) + color;
  return out;
}

fragment float4 fragmentMain(float4 inColor : attribute(1)) : SV_Target 0 {
    return inColor;
}
`;

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

            const gpuContext = this.element.getContext('gpu');

            navigator.gpu.requestAdapter({ powerPreference: "low-power" }).then(adapter => {
                return adapter.requestDevice().then(device => {
                    this._device = device;

                    const swapChainFormat = "bgra8unorm";
                    this._swapChain = gpuContext.configureSwapChain({
                        device: device,
                        format: swapChainFormat,
                        usage: GPUTextureUsage.OUTPUT_ATTACHMENT
                    });

                    this._timeBindGroupLayout = device.createBindGroupLayout({
                        bindings: [
                            { binding: 0, visibility: GPUShaderStageBit.VERTEX, type: "uniform-buffer" },
                        ],
                    });

                    this._bindGroupLayout = device.createBindGroupLayout({
                        bindings: [
                            { binding: 0, visibility: GPUShaderStageBit.VERTEX, type: "uniform-buffer" },
                        ],
                    });

                    const vec4Size = 4 * Float32Array.BYTES_PER_ELEMENT;

                    const pipelineLayout = device.createPipelineLayout({ bindGroupLayouts: [this._timeBindGroupLayout, this._bindGroupLayout] });
                    const shaderModule = device.createShaderModule({ code: whlslShaders, isWHLSL: true });

                    const pipelineDesc = {
                        layout: pipelineLayout,
                        vertexStage: {
                            module: shaderModule,
                            entryPoint: "vertexMain",
                        },
                        fragmentStage: {
                            module: shaderModule,
                            entryPoint: "fragmentMain"
                        },

                        primitiveTopology: "triangle-list",

                        vertexInput: {
                            indexFormat: "uint32",
                            vertexBuffers: [{
                                // vertex buffer
                                stride: 2 * vec4Size,
                                stepMode: "vertex",
                                attributeSet: [{
                                    // vertex positions
                                    shaderLocation: 0,
                                    offset: 0,
                                    format: "float4"
                                }, {
                                    // vertex colors
                                    shaderLocation: 1,
                                    offset: vec4Size,
                                    format: "float4"
                                }],
                            }],
                        },

                        rasterizationState: {
                            frontFace: 'ccw',
                            cullMode: 'none',
                        },

                        colorStates: [{
                            format: swapChainFormat,
                            alphaBlend: {},
                            colorBlend: {},
                        }],
                    };

                    this._pipeline = device.createRenderPipeline(pipelineDesc);

                    const [vertexBuffer, vertexArrayBuffer] = device.createBufferMapped({
                        size: 2 * 3 * vec4Size,
                        usage: GPUBufferUsage.VERTEX
                    });
                    const vertexWriteBuffer = new Float32Array(vertexArrayBuffer);
                    vertexWriteBuffer.set([
                    // position data  /**/ color data
                    0, 0.1, 0, 1,     /**/ 1, 0, 0, 1,
                    -0.1, -0.1, 0, 1, /**/ 0, 1, 0, 1,
                    0.1, -0.1, 0, 1,  /**/ 0, 0, 1, 1,
                    ]);
                    vertexBuffer.unmap();

                    this._vertexBuffer = vertexBuffer;
                    this._timeMappedBuffers = [];

                    this._resetIfNecessary();

                    benchmark._initPromise.resolve();
                });
            });
        },

        _getFunctionSource: function(id)
        {
            return document.getElementById(id).text;
        },

        _resetIfNecessary: function()
        {
            if (this._bindGroups != undefined && this._numTriangles <= this._bindGroups.length)
                return;

            const numTriangles = this._numTriangles;

            const device = this._device;

            // Minimum buffer offset alignment is 256 bytes.
            const uniformBytes = 5 * Float32Array.BYTES_PER_ELEMENT;
            const alignedUniformBytes = Math.ceil(uniformBytes / 256) * 256;
            const alignedUniformFloats = alignedUniformBytes / Float32Array.BYTES_PER_ELEMENT;

            const [uniformBuffer, uniformArrayBuffer] = device.createBufferMapped({
                size: numTriangles * alignedUniformBytes + Float32Array.BYTES_PER_ELEMENT,
                usage: GPUBufferUsage.TRANSFER_DST | GPUBufferUsage.UNIFORM
            });
            const uniformWriteArray = new Float32Array(uniformArrayBuffer);

            this._bindGroups = new Array(numTriangles);
            for (let i = 0; i < numTriangles; ++i) {
                uniformWriteArray[alignedUniformFloats * i + 0] = Stage.random(0.2, 0.4);   // scale
                uniformWriteArray[alignedUniformFloats * i + 1] = Stage.random(-0.9, 0.9);  // offsetX
                uniformWriteArray[alignedUniformFloats * i + 2] = Stage.random(-0.9, 0.9);  // offsetY
                uniformWriteArray[alignedUniformFloats * i + 3] = Stage.random(0.5, 2);     // scalar
                uniformWriteArray[alignedUniformFloats * i + 4] = Stage.random(0, 10);      // scalarOffset

                this._bindGroups[i] = device.createBindGroup({
                  layout: this._bindGroupLayout,
                  bindings: [{
                    binding: 0,
                    resource: {
                      buffer: uniformBuffer,
                      offset: i * alignedUniformBytes,
                      size: 6 * Float32Array.BYTES_PER_ELEMENT,
                    }
                  }]
                });
            }

            uniformBuffer.unmap();

            this._timeOffset = numTriangles * alignedUniformBytes;
            this._timeBindGroup = device.createBindGroup({
            layout: this._timeBindGroupLayout,
            bindings: [{
                binding: 0,
                resource: {
                    buffer: uniformBuffer,
                    offset: this._timeOffset,
                    size: Float32Array.BYTES_PER_ELEMENT,
                    }
                }]
            });

            this._uniformBuffer = uniformBuffer;
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
            const device = this._device;

            if (!this._startTime)
                this._startTime = Stage.dateCounterValue(1000);

            const elapsedTimeData = new Float32Array([Stage.dateCounterValue(1000) - this._startTime]);

            // Update time uniform
            let mappedBuffer;

            if (this._timeMappedBuffers.length === 0) {
                mappedBuffer = device.createBufferMapped({
                    size: Float32Array.BYTES_PER_ELEMENT,
                    usage: GPUBufferUsage.TRANSFER_SRC | GPUBufferUsage.MAP_WRITE
                });
            } else
                mappedBuffer = this._timeMappedBuffers.shift();

            const [timeStagingBuffer, timeStagingArrayBuffer] = mappedBuffer;

            const writeArray = new Float32Array(timeStagingArrayBuffer);
            writeArray.set(elapsedTimeData);
            timeStagingBuffer.unmap();

            const commandEncoder = device.createCommandEncoder({});
            commandEncoder.copyBufferToBuffer(timeStagingBuffer, 0, this._uniformBuffer, this._timeOffset, elapsedTimeData.byteLength);

            const renderPassDescriptor = {
                colorAttachments: [{
                    loadOp: "clear",
                    storeOp: "store",
                    clearColor: { r: 1, g: 1, b: 1, a: 1.0 },
                    attachment: this._swapChain.getCurrentTexture().createDefaultView(),
                }],
            };

            const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
            passEncoder.setPipeline(this._pipeline);
            passEncoder.setVertexBuffers(0, [this._vertexBuffer], [0]);
            passEncoder.setBindGroup(0, this._timeBindGroup);
            for (let i = 0; i < this._numTriangles; ++i) {
                passEncoder.setBindGroup(1, this._bindGroups[i]);
                passEncoder.draw(3, 1, 0, 0);
            }
            passEncoder.endPass();

            device.getQueue().submit([commandEncoder.finish()]);

            timeStagingBuffer.mapWriteAsync().then(arrayBuffer => {
                mappedBuffer[1] = arrayBuffer;
                this._timeMappedBuffers.push(mappedBuffer);
            });
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
    }, {

    waitUntilReady: function() {
        this._initPromise = new SimplePromise;
        return this._initPromise;
    },
});

window.benchmarkClass = WebGLBenchmark;

})();
