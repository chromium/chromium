luci.console_view(
    name = 'chromium.gpu.fyi',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/GPU FYI Win Builder',
            category = 'Windows|Builder|Release',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Win x64 Builder',
            category = 'Windows|Builder|Release',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Win dEQP Builder',
            category = 'Windows|Builder|dEQP',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Win x64 dEQP Builder',
            category = 'Windows|Builder|dEQP',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Win x64 DX12 Vulkan Builder',
            category = 'Windows|Builder|dx12vk',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Win x64 DX12 Vulkan Builder (dbg)',
            category = 'Windows|Builder|dx12vk',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Win Builder (dbg)',
            category = 'Windows|Builder|Debug',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Win x64 Builder (dbg)',
            category = 'Windows|Builder|Debug',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI XR Win x64 Builder',
            category = 'Windows|Builder|XR',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 FYI x64 Debug (NVIDIA)',
            category = 'Windows|10|x64|Nvidia',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 FYI x64 DX12 Vulkan Debug (NVIDIA)',
            category = 'Windows|10|x64|Nvidia|dx12vk',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 FYI x64 DX12 Vulkan Release (NVIDIA)',
            category = 'Windows|10|x64|Nvidia|dx12vk',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 FYI x64 Release (Intel HD 630)',
            category = 'Windows|10|x64|Intel',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 FYI x64 Release (Intel UHD 630)',
            category = 'Windows|10|x64|Intel',
            short_name = 'uhd',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 FYI x64 Release (NVIDIA)',
            category = 'Windows|10|x64|Nvidia',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 FYI x64 Release XR Perf (NVIDIA)',
            category = 'Windows|10|x64|Nvidia',
            short_name = 'xr',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 FYI x64 dEQP Release (Intel HD 630)',
            category = 'Windows|10|x64|Intel',
            short_name = 'dqp',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 FYI x64 dEQP Release (NVIDIA)',
            category = 'Windows|10|x64|Nvidia',
            short_name = 'dqp',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 FYI x64 Exp Release (Intel HD 630)',
            category = 'Windows|10|x64|Intel',
            short_name = 'exp',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 FYI x64 Exp Release (NVIDIA)',
            category = 'Windows|10|x64|Nvidia',
            short_name = 'exp',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 FYI x64 Release (AMD RX 550)',
            category = 'Windows|10|x64|AMD',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 FYI x64 Release (NVIDIA GeForce GTX 1660)',
            category = 'Windows|10|x64|Nvidia',
            short_name = 'gtx',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 FYI x64 SkiaRenderer GL (NVIDIA)',
            category = 'Windows|10|x64|Nvidia',
            short_name = 'skgl',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 FYI x86 Release (NVIDIA)',
            category = 'Windows|10|x86|Nvidia',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Win7 FYI Debug (AMD)',
            category = 'Windows|7|x86|AMD',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Win7 FYI dEQP Release (AMD)',
            category = 'Windows|7|x86|AMD',
            short_name = 'dqp',
        ),
        luci.console_view_entry(
            builder = 'ci/Win7 FYI Release (AMD)',
            category = 'Windows|7|x86|AMD',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Win7 FYI Release (NVIDIA)',
            category = 'Windows|7|x86|Nvidia',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Win7 FYI x64 Release (NVIDIA)',
            category = 'Windows|7|x64|Nvidia',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Win7 FYI x64 dEQP Release (NVIDIA)',
            category = 'Windows|7|x64|Nvidia',
            short_name = 'dqp',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Mac Builder',
            category = 'Mac|Builder',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Mac Builder (dbg)',
            category = 'Mac|Builder',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Mac dEQP Builder',
            category = 'Mac|Builder',
            short_name = 'dqp',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac FYI Debug (Intel)',
            category = 'Mac|Intel',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac FYI Experimental Release (Intel)',
            category = 'Mac|Intel',
            short_name = 'exp',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac FYI dEQP Release Intel',
            category = 'Mac|Intel',
            short_name = 'dqp',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac FYI Release (Intel)',
            category = 'Mac|Intel',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac Pro FYI Release (AMD)',
            category = 'Mac|AMD|Pro',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac FYI dEQP Release AMD',
            category = 'Mac|AMD',
            short_name = 'dqp',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac FYI Retina Debug (AMD)',
            category = 'Mac|AMD|Retina',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac FYI Retina Release (AMD)',
            category = 'Mac|AMD|Retina',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac FYI Experimental Retina Release (AMD)',
            category = 'Mac|AMD|Retina',
            short_name = 'exp',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac FYI Retina Debug (NVIDIA)',
            category = 'Mac|Nvidia',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac FYI Retina Release (NVIDIA)',
            category = 'Mac|Nvidia',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac FYI Experimental Retina Release (NVIDIA)',
            category = 'Mac|Nvidia',
            short_name = 'exp',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac FYI GPU ASAN Release',
            category = 'Mac',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Linux Builder',
            category = 'Linux|Builder',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Linux Builder (dbg)',
            category = 'Linux|Builder',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Linux Ozone Builder',
            category = 'Linux|Builder',
            short_name = 'ozn',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Linux dEQP Builder',
            category = 'Linux|Builder',
            short_name = 'dqp',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux FYI Experimental Release (Intel HD 630)',
            category = 'Linux|Intel',
            short_name = 'exp',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux FYI Release (Intel HD 630)',
            category = 'Linux|Intel',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux FYI Release (Intel UHD 630)',
            category = 'Linux|Intel',
            short_name = 'uhd',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux FYI dEQP Release (Intel HD 630)',
            category = 'Linux|Intel',
            short_name = 'dqp',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux FYI Ozone (Intel)',
            category = 'Linux|Intel',
            short_name = 'ozn',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux FYI Release (AMD R7 240)',
            category = 'Linux',
            short_name = 'amd',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux FYI Release (NVIDIA)',
            category = 'Linux|Nvidia',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux FYI Experimental Release (NVIDIA)',
            category = 'Linux|Nvidia',
            short_name = 'exp',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux FYI Debug (NVIDIA)',
            category = 'Linux|Nvidia',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux FYI dEQP Release (NVIDIA)',
            category = 'Linux|Nvidia',
            short_name = 'dqp',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux FYI SkiaRenderer Vulkan (Intel HD 630)',
            category = 'Linux|Intel',
            short_name = 'skv',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux FYI SkiaRenderer Vulkan (NVIDIA)',
            category = 'Linux|Nvidia',
            short_name = 'skv',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux FYI GPU TSAN Release',
            category = 'Linux',
            short_name = 'tsn',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI Release (Nexus 5)',
            category = 'Android|L32',
            short_name = 'N5',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI Release (Nexus 6)',
            category = 'Android|L32',
            short_name = 'N6',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI Release (Nexus 5X)',
            category = 'Android|M64|QCOM',
            short_name = 'N5X',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI Release (Nexus 6P)',
            category = 'Android|M64|QCOM',
            short_name = 'N6P',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI Release (Nexus 9)',
            category = 'Android|M64|NVDA',
            short_name = 'N9',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI Release (NVIDIA Shield TV)',
            category = 'Android|N64|NVDA',
            short_name = 'STV',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI Release (Pixel 2)',
            category = 'Android|P32|QCOM',
            short_name = 'P2',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI 32 Vk Release (Pixel 2)',
            category = 'Android|vk|Q32',
            short_name = 'P2',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI 64 Vk Release (Pixel 2)',
            category = 'Android|vk|Q64',
            short_name = 'P2',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI dEQP Release (Nexus 5X)',
            category = 'Android|dqp|M64',
            short_name = 'N5X',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI 32 dEQP Vk Release (Pixel 2)',
            category = 'Android|dqp|vk|Q32',
            short_name = 'P2',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI 64 dEQP Vk Release (Pixel 2)',
            category = 'Android|dqp|vk|Q64',
            short_name = 'P2',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI SkiaRenderer GL (Nexus 5X)',
            category = 'Android|skgl|M64',
            short_name = 'N5X',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI SkiaRenderer Vulkan (Pixel 2)',
            category = 'Android|skv|P32',
            short_name = 'P2',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Perf Android 64 Builder',
            category = 'Android|Perf|Builder',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI 64 Perf (Pixel 2)',
            category = 'Android|Perf|Q64',
            short_name = 'P2',
        ),
    ],
)
