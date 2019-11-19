// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/url_loader_factory_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/hash/sha1.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/cors_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/switches.h"
#include "extensions/common/user_script.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace extensions {

namespace {

enum class FactoryUser {
  kContentScript,
  kExtensionProcess,
};

// The allowlist contains HashedExtensionId of extensions discovered via
// Extensions.CrossOriginFetchFromContentScript2 Rappor data.  When the data was
// gathered, these extensions relied on making cross-origin requests from
// content scripts (which requires relaxing CORB).  Going forward, these
// extensions should migrate to making those requests from elsewhere (e.g. from
// a background page, or in the future from extension service workers) at which
// point they can be removed from the allowlist.
//
// Migration plan for extension developers is described at
// https://chromium.org/Home/chromium-security/extension-content-script-fetches
const char* kHardcodedPartOfCorbAllowlist[] = {
    "039F93DD1DF836F1D4E2084C1BEFDB46A854A9D1",
    "03E5D80A49C309F7B55ED6BD2B0EDEB38021ED4E",
    "072D729E856B1F2C9894AEEC3A5DF65E519D6BEE",
    "07333481B7B8D7F57A9BA64FB98CF86EA87455FC",
    "086E69ED9071DCB20C93A081A68360963AB09385",
    "0872D01871BC7F7428E075D656874AD43580B575",
    "09386608C84745F0A05AC8C97165D8D76AC83771",
    "0A8E468BCEAA5626207AB77C431C31C1F2A8F76A",
    "0C011D916B15E5451E1B84BD14397B8EC98F455B",
    "0CB16BAEE070B7617E9188B387C44964FB705D79",
    "0D6C5E12B5D03639257D4C83AE1C27ECF1419C98",
    "0EAEA2FDEE025D95B3ABB37014EFF5A98AC4BEAE",
    "0FCD1282065485458E630683F098F591B24C406D",
    "109A37B889E7C8AEA7B0103559C3EB6AF73B7832",
    "11AF49711EE340C366CC820C98E3CA2AD7D7DE07",
    "16A81AEA09A67B03F7AEA5B957D24A4095E764BE",
    "1AB9CC404876117F49135E67BAD813F935AAE9BA",
    "1B9251EF3EDD5A2B2872B168406F36FB18C72F37",
    "1CDDF7436E5F891E1D5E37164F7EB992AECA0E2D",
    "1DB115A4344B58C0B7CC1595662E86C9E92C0848",
    "1DF1ED7172DC5B9FF337909ED5C32DED74084825",
    "1E37F1A19C1C528E616637B105CFC4838ECF52B4",
    "24B24CBA3D4B395503F9B0D6286E55E0FDDE4D69",
    "260871EABEDE6F2D07D0D9450096093EDAFCBF34",
    "28A9EDFD65BC27B5048E516AA7165339F5ACBB30",
    "2A661509BCE9F8384B00CFC96807597D71DFE94C",
    "2AA94E2D3F4DA33F0D3BCF5DD48F69B8BDB26F52",
    "2E2D8A405430172AB15ADCC09740F3EEE990D605",
    "30E95BD31B11118CE488EB4FC5FF7135E0C59425",
    "31E6100DC7B4EAB4ABF6CA2A4E191D3945D3C731",
    "3230014EA01150A27C1887B700E019B27A6DBA08",
    "34FB670464B5F16EF5ABD6CD53E26030E97C26B3",
    "360D8A88545D0C21CBDE2B55EA9E4E4285282B4C",
    "3787567233ED6BACC4FC05387BE30E03434CE4CC",
    "37AC33A3A46D271CCE57DD6CB3FACE6B01F5A347",
    "38B4D1CC339580F506BC86D4027A49721AFB4BB9",
    "395D84F94DA287C0E4DBAF9ACE478B9710C0029F",
    "3A2E664CA697C622EA4CFA40373B3E641C01713B",
    "3BC834B48C2C13765147FBAD710F792F026378D8",
    "3CD98763C80D86E00CB1C4CAA56CEA8F3B0BA4F1",
    "3EB17C39F8B6B28FAF34E2406E57A76013A2E066",
    "3FDD3DB17F3B686F5A05204700ABA13DF20AE957",
    "41536C3554CD9458EB2F05F1B58CF84BB7BF83BC",
    "42C96DF87C997828C185DAD247F469940DAFC2CD",
    "43865F8D555C201FE452A5A40702EA96240E749C",
    "44943FADD66932EF56EE3D856A9FAAD4A8AF0FD9",
    "4633828B42F673E0E62475059526DC2C24091690",
    "47ADBB376050C083FFC54CC28CD3D1F54BF0BFED",
    "4884C48FB64665BD86509BA1F2D22D75D61ACBF9",
    "4913450195D177430217CAB64B356DC6F6B0F483",
    "49FFDF2212E50090963E33159DBF853B5C475340",
    "4A15AD9F12FB35BEEA6FF1807AC782A1DF979387",
    "4A4E0B370B65C9EC65C8A615B1B9FB55A1B3E1FD",
    "4A8EEEB4754A3E87C3A177CB31474A30967CB3C8",
    "4E4167EDA0CFF22F261C0655E979B9474BF67C04",
    "5053323D1F7B6EEC97A77A350DB6D0D8E51CD0AC",
    "505F2C1E723731B2C8C9182FEAA73D00525B2048",
    "505F3697087BFD3F290F42D029CE67F1C793B2DA",
    "50DDD8734521B61564FCE273F8E60547F88BBCBE",
    "52865B2087D0ABCD195A83DFD4BD041A3B4EBC34",
    "52C94AC7680C3A03CCB6EA31445DD42BD0D5CA8E",
    "537D26CC4EC6819A6CAAE8B8B5957F5ECFA7A44B",
    "5741A650C3AAA935154C57681310805B5FC121C3",
    "58BCF05A42C8ECED4E6D76F51E2E1A64AC4F7E7C",
    "5B6DC4EEFEDF06CB2BF439E4607A5BAF6E45CCA1",
    "5E052881B4847F68CFC8ED1A00C341FC14009C1E",
    "5F0C47BC039BEDC1B29B68918F75370C292076A6",
    "61E581B10D83C0AEF8366BB64C18C6615884B3D2",
    "628FDD7CB49D46CF545AB9D46B3188802104F973",
    "6357533CAFFB94A9EA5268ED110079E15561E469",
    "65C20C06ED10E6F39EED527AC736D87B0390DE70",
    "67528F9B47BE454EC46809C33D24F2C199BE408D",
    "68F43E671577CF03AE901A5780DC07879331A3A9",
    "6A113D4E2F96997D9BA4B391B90ED51058B37EFF",
    "6AE81EF3B13B15080A2DDB23A205A24C65CCC10B",
    "6D3A671DABC1F87910A1AA67EE85C02095D35624",
    "6E49449D56D031B87835CC767734AF5A064E1A13",
    "6FD56FE5B3831B17ED5301EE2EF949CEE5BFA871",
    "71351EAA5C16350EC5A86C23D7A288317309E53D",
    "71CB78C3334D5122E7F23C8525AD24100CDE7D4A",
    "71EE66C0F71CD89BEE340F8568A44101D4C3A9A7",
    "7527942941BFF13D66B46E7A2A56FDBA873FB9E6",
    "77D83E0A4157A0E77B51AD60BAB69A346CD4FEA3",
    "7879DB88205D880B64D55E51B9726E1D12F7261F",
    "7B179178FD3246EF29212093F1025A11C0F127ED",
    "7BFE588B209A15260DE12777B4BBB738DE98FE6C",
    "7C9DEE7EABBF6C722DC7C1B86460F0507E5AA561",
    "808FA9BB3CD501D7801D1CD6D5A3DBA088FDD46F",
    "81FD24AF95679B900370DB857CE2EACADBE50A9B",
    "82FDBBF79F3517C3946BD89EAAF90C46DFDA4681",
    "83431421F759AE7A3BDAC00A4959D13095C65805",
    "834BD6E8E9F59D388DBB264453EB08A5DE45ED03",
    "83B6C75264D5D2F81FDEFD681EDD2076DD8F0B9B",
    "88C372CE52E21560C17BFD52556E60D694E12CAC",
    "88F5F459139892C0F5DF3022676726BB3F01FB5C",
    "896C60AC2F7F03B4F582027DD2107F3BB67DF69D",
    "89C9B32115F19A18E9BE4906DC59F24A934CB9F0",
    "89F40D84C0C72C6B02B320716E877FB1671218E9",
    "8A0634388BCBB6D073E1C97B14C024396ED32D12",
    "8CDD303D6C341D9CAB16713C3CD7587B90A7A84A",
    "8CE6227B4E53DF42FF93B24F49D15EDE31E97E79",
    "905C225238FB337834F193DF48550D721445B438",
    "9233B7F0B98FCC181ED43AFAF58056C4EDCED162",
    "92F2B155490417F0797849C81292E3986EBE6811",
    "934B8F5753A3E5A276FC7D0EE5E575B335A0CC76",
    "93934B0B87347437699EB62A8921F59F40C36D7A",
    "93BBF911E8871F6FCC8170448FD2DF5B9EF233E5",
    "95E78675D2DB61DC688586CD7A24202A260907A4",
    "973E35633030AD27DABEC99609424A61386C7309",
    "9784343657207FE88A629E8EAA7A4A19C7C8CBE0",
    "97E04C5632954E778306CAC40B3F95C470B463B6",
    "999BD8D1929F9ABB817E9368480D93BAB2A0983D",
    "99E06C364BBB2D1F82A9D20BC1645BF21E478259",
    "9C6A186F8D3C5FD0CC8DCF49682FA726BD8A7705",
    "A04F08A772F1C83B7A14ED29788ACA4F000BBE05",
    "A059797AECB77D24DEB248C3413D99B0D3BF9A8C",
    "A07DA0EDB967D027E3B220208AD085FDC44C3231",
    "A3660FA31A0DBF07C9F80D5342FF215DBC962719",
    "A42FE007E0651EAC159EE1B393586AFFFE065DC2",
    "A6057397EDC4E6CF25BC3A142F866ACC653B1CF1",
    "A733063124AC9E1E6E1E331FFBAAE32D81AC2581",
    "A8FB3967ADE404B77AC3FB5815A399C0660C3C63",
    "A9A4B26C2387BA2A5861C790B0FF39F230427AC8",
    "AA3DE48E23B2465B21F5D33E993FD959F611DD10",
    "ADD14F4517B9A87D4E841369417E5BDB5FDFF263",
    "AE063CF9FF5D718AD6F1CF242FABAC39B57ADEBA",
    "AEEDAC793F184240CFB800DA73EE6321E5145102",
    "AF0965B74237AFF383C981C05178732C9A05A140",
    "B3CF6C01796E8D03378FAA77AF507E27BB847E9D",
    "B4782AE831D849EFCC2AF4BE2012816EDDF8D908",
    "B6903E9A5A8CC5D74A688DA0A67AFA2B0944F605",
    "BF5224FB246A6B67EA986EFF77A43F6C1BCA9672",
    "C0A30989F3717CE5B1B2FE462797951EA6D3922A",
    "C4A81852B9ACE6CE02DAB58BB77BDA0AD75716EC",
    "C5539F4EBECABA792CC40D03A56144AAD3BF9D19",
    "C86D546CA47034163C12DC2C912910C3A12C3B07",
    "C940F83135D9612865F4A44391DDDFE3B7BE1393",
    "CA89BD35059845F2DB4B4398FD339B9F210E9337",
    "CC32A0FD1D88B403308EACBE4DE3CA5AC54B93EB",
    "CD8AF9C47DDE6327F8D9A3EFA81F34C6B6C26EBB",
    "CF40F6289951CBFA3B83B792EFA774E2EA06E4C0",
    "D347F78F32567E90BC32D9C16B085254EA269590",
    "D572BE31227F6D0BE95B9430BE2D5F21D7D9CF9A",
    "D7C3879A8898618E3A23B0E6BFB6A38D01606246",
    "DC39837AC518B832FCB2D2DC1CE8BA148F54758E",
    "DC88B4C9E547F3E321B3E64CCDBD4B698116D2F4",
    "DDA21167F058A65D878DF84C3CF3FCC60B053E80",
    "E134BC4A0FF6C59CE42CC76BA6B2D6F5DC648EC4",
    "E14510DB95CB9E60B4C8CB10B0AC9DE837B5D7D7",
    "E178D4F4D6617C0B880C36F192DA3B18422C5064",
    "E61DC62BF2F1D7CAEEF93E84A5EE5F6D2CFDBC79",
    "E6B12430B6166B31BE20E13941C22569EA75B0F2",
    "E7036E906DBFB77C46EDDEB003A72C0B5CC9BE7F",
    "E8309B92AACFEB64D39F5466EF3BDB6B63A7248B",
    "E873675B8E754F1B1F1222CB921EA14B4335179D",
    "EAD0BFA4ED66D9A2112A4C9A0AA25329FF980AC6",
    "EC24668224116D19FF1A5FFAA61238B88773982C",
    "EC4A841BD03C8E5202043165188A9E060BF703A3",
    "EE4BE5F23D2E59E4713958465941EFB4A18166B7",
    "EE711E704D4A365C4644EE4637076C81DF454EA6",
    "F1ACA279F460440E47078D91FE372212DD9B8709",
    "F273C23C616F5C56E8EDBAE24B21F5D408936A0D",
    "F566B33D62CE21415AF5B3F3FD8762B7454B8874",
    "F59AB261280AB3AE9826D9359507838B90B07431",
    "F608282162AD48CE45D5BC2F6F467B56E88EBFA4",
    "F73F9EF0207603992CA3C00A7A0CB223D5571B3F",
    "F9287A33E15038F2591F23E6E9C486717C7202DD",
    "FCC2DC6574A3CA28ED77195926C67F612292C5C3",
    "FEE3DC8C722657A4A5B0F72CA48CF950DC956148",
    "FF0DA4BD87A88469B10709B99E79D4B0E11C0CA6",
    "FF8257C73304BA655E10F324C962504BA6691DF2",
};

constexpr size_t kHashedExtensionIdLength = base::kSHA1Length * 2;
bool IsValidHashedExtensionId(const std::string& hash) {
  bool correct_chars = std::all_of(hash.begin(), hash.end(), [](char c) {
    return ('A' <= c && c <= 'F') || ('0' <= c && c <= '9');
  });
  bool correct_length = (kHashedExtensionIdLength == hash.length());
  return correct_chars && correct_length;
}

std::vector<std::string> CreateExtensionAllowlist() {
  std::vector<std::string> allowlist;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceEmptyCorbAllowlist)) {
    return allowlist;
  }

  // Append extensions from the hardcoded allowlist.
  allowlist.reserve(base::size(kHardcodedPartOfCorbAllowlist));
  for (const char* hash : kHardcodedPartOfCorbAllowlist) {
    DCHECK(IsValidHashedExtensionId(hash));  // It also validates the length.
    allowlist.push_back(std::string(hash, kHashedExtensionIdLength));
  }

  return allowlist;
}

// Returns a set of HashedExtensionId of extensions that depend on relaxed CORB
// behavior in their content scripts.
base::flat_set<std::string>& GetExtensionsAllowlist() {
  static base::NoDestructor<base::flat_set<std::string>> s_allowlist([] {
    base::flat_set<std::string> result(CreateExtensionAllowlist());
    result.shrink_to_fit();
    return result;
  }());
  return *s_allowlist;
}

bool DoContentScriptsDependOnRelaxedCorb(const Extension& extension) {
  // Content scripts injected by Chrome Apps (e.g. into <webview> tag) need to
  // run with relaxed CORB.
  if (extension.is_platform_app())
    return true;

  // Content scripts in the current version of extensions might depend on
  // relaxed CORB.
  if (extension.manifest_version() <= 2) {
    const std::string& hash = extension.hashed_id().value();
    DCHECK(IsValidHashedExtensionId(hash));
    return base::Contains(GetExtensionsAllowlist(), hash);
  }

  // Safe fallback for future extension manifest versions.
  return false;
}

bool DoExtensionPermissionsCoverCorsOrCorbRelatedOrigins(
    const Extension& extension) {
  // TODO(lukasza): https://crbug.com/1016904: Return false if the |extension|
  // doesn't need a special URLLoaderFactory based on |extension| permissions.
  // For now we conservatively assume that all extensions need relaxed CORS/CORB
  // treatment.
  return true;
}

bool IsSpecialURLLoaderFactoryRequired(const Extension& extension,
                                       FactoryUser factory_user) {
  switch (factory_user) {
    case FactoryUser::kContentScript:
      return DoContentScriptsDependOnRelaxedCorb(extension) &&
             DoExtensionPermissionsCoverCorsOrCorbRelatedOrigins(extension);
    case FactoryUser::kExtensionProcess:
      return DoExtensionPermissionsCoverCorsOrCorbRelatedOrigins(extension);
  }
}

mojo::PendingRemote<network::mojom::URLLoaderFactory> CreateURLLoaderFactory(
    content::RenderProcessHost* process,
    network::mojom::NetworkContext* network_context,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    const Extension& extension,
    const url::Origin& main_world_origin,
    const base::Optional<net::NetworkIsolationKey>& network_isolation_key) {
  // Compute relaxed CORB config to be used by |extension|.
  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();

  params->network_isolation_key = network_isolation_key;

  // Setup factory bound allow list that overwrites per-profile common list
  // to allow tab specific permissions only for this newly created factory.
  params->factory_bound_access_patterns =
      network::mojom::CorsOriginAccessPatterns::New();
  params->factory_bound_access_patterns->source_origin =
      url::Origin::Create(extension.url());
  params->factory_bound_access_patterns->allow_patterns =
      CreateCorsOriginAccessAllowList(
          extension,
          PermissionsData::EffectiveHostPermissionsMode::kIncludeTabSpecific);

  if (header_client)
    params->header_client = std::move(*header_client);
  params->process_id = process->GetID();
  // TODO(lukasza): https://crbug.com/1016904: Use more granular CORB
  // enforcement based on the specific |extension|'s permissions.
  params->is_corb_enabled = false;
  params->request_initiator_site_lock = main_world_origin;

  // Create the URLLoaderFactory.
  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  network_context->CreateURLLoaderFactory(
      factory_remote.InitWithNewPipeAndPassReceiver(), std::move(params));
  return factory_remote;
}

void MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
    content::RenderFrameHost* frame,
    std::vector<url::Origin> request_initiators,
    bool push_to_renderer_now) {
  DCHECK(!request_initiators.empty());
  frame->MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
      std::move(request_initiators), push_to_renderer_now);
}

// If |match_about_blank| is true, then traverses parent/opener chain until the
// first non-about-scheme document and returns its url.  Otherwise, simply
// returns |document_url|.
//
// This function approximates ScriptContext::GetEffectiveDocumentURL from the
// renderer side.  Unlike the renderer version of this code (in
// ScriptContext::GetEffectiveDocumentURL) the code below doesn't consider
// whether security origin of |frame| can access |next_candidate|.  This is
// okay, because our only caller (DoesContentScriptMatchNavigatingFrame) expects
// false positives.
GURL GetEffectiveDocumentURL(content::RenderFrameHost* frame,
                             const GURL& document_url,
                             bool match_about_blank) {
  base::flat_set<content::RenderFrameHost*> already_visited_frames;

  // Common scenario. If |match_about_blank| is false (as is the case in most
  // extensions), or if the frame is not an about:-page, just return
  // |document_url| (supposedly the URL of the frame).
  if (!match_about_blank || !document_url.SchemeIs(url::kAboutScheme))
    return document_url;

  // Non-sandboxed about:blank and about:srcdoc pages inherit their security
  // origin from their parent frame/window. So, traverse the frame/window
  // hierarchy to find the closest non-about:-page and return its URL.
  content::RenderFrameHost* found_frame = frame;
  do {
    DCHECK(found_frame);
    already_visited_frames.insert(found_frame);

    // The loop should only execute (and consider the parent chain) if the
    // currently considered frame has about: scheme.
    DCHECK(match_about_blank);
    DCHECK(
        ((found_frame == frame) && document_url.SchemeIs(url::kAboutScheme)) ||
        (found_frame->GetLastCommittedURL().SchemeIs(url::kAboutScheme)));

    // Attempt to find |next_candidate| - either a parent of opener of
    // |found_frame|.
    content::RenderFrameHost* next_candidate = found_frame->GetParent();
    if (!next_candidate) {
      next_candidate =
          content::WebContents::FromRenderFrameHost(found_frame)->GetOpener();
    }
    if (!next_candidate ||
        base::Contains(already_visited_frames, next_candidate)) {
      break;
    }

    found_frame = next_candidate;
  } while (found_frame->GetLastCommittedURL().SchemeIs(url::kAboutScheme));

  if (found_frame == frame)
    return document_url;  // Not committed yet at ReadyToCommitNavigation time.
  return found_frame->GetLastCommittedURL();
}

// If |user_script| will inject JavaScript content script into the target of
// |navigation|, then DoesContentScriptMatchNavigatingFrame returns true.
// Otherwise it may return either true or false.  Note that this function
// ignores CSS content scripts.
//
// This function approximates a subset of checks from
// UserScriptSet::GetInjectionForScript (which runs in the renderer process).
// Unlike the renderer version, the code below doesn't consider ability to
// create an injection host or the results of ScriptInjector::CanExecuteOnFrame.
// Additionally the |effective_url| calculations are also only an approximation.
// This is okay, because we may return either true even if no content scripts
// would be injected (i.e. it is okay to create a special URLLoaderFactory when
// in reality the content script won't be injected and won't need the factory).
bool DoesContentScriptMatchNavigatingFrame(
    const UserScript& user_script,
    content::RenderFrameHost* navigating_frame,
    const GURL& navigation_target) {
  // A special URLLoaderFactory is only needed for Javascript content scripts
  // (and is never needed for CSS-only injections).
  if (user_script.js_scripts().empty())
    return false;

  GURL effective_url = GetEffectiveDocumentURL(
      navigating_frame, navigation_target, user_script.match_about_blank());
  bool is_subframe = navigating_frame->GetParent();
  return user_script.MatchesDocument(effective_url, is_subframe);
}

}  // namespace

// static
bool URLLoaderFactoryManager::DoContentScriptsMatchNavigatingFrame(
    const Extension& extension,
    content::RenderFrameHost* navigating_frame,
    const GURL& navigation_target) {
  const UserScriptList& list =
      ContentScriptsInfo::GetContentScripts(&extension);
  return std::any_of(list.begin(), list.end(),
                     [navigating_frame, navigation_target](
                         const std::unique_ptr<UserScript>& script) {
                       return DoesContentScriptMatchNavigatingFrame(
                           *script, navigating_frame, navigation_target);
                     });
}

// static
void URLLoaderFactoryManager::ReadyToCommitNavigation(
    content::NavigationHandle* navigation) {
  content::RenderFrameHost* frame = navigation->GetRenderFrameHost();
  const GURL& url = navigation->GetURL();

  std::vector<url::Origin> initiators_requiring_separate_factory;
  const ExtensionRegistry* registry =
      ExtensionRegistry::Get(frame->GetProcess()->GetBrowserContext());
  DCHECK(registry);  // ReadyToCommitNavigation shouldn't run during shutdown.
  for (const auto& it : registry->enabled_extensions()) {
    const Extension& extension = *it;
    if (!DoContentScriptsMatchNavigatingFrame(extension, frame, url))
      continue;

    if (!IsSpecialURLLoaderFactoryRequired(extension,
                                           FactoryUser::kContentScript))
      continue;

    initiators_requiring_separate_factory.push_back(
        url::Origin::Create(extension.url()));
  }

  if (!initiators_requiring_separate_factory.empty()) {
    // At ReadyToCommitNavigation time there is no need to trigger an explicit
    // push of URLLoaderFactoryBundle to the renderer - it is sufficient if the
    // factories are pushed slightly later - during the commit.
    constexpr bool kPushToRendererNow = false;

    MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
        frame, std::move(initiators_requiring_separate_factory),
        kPushToRendererNow);
  }
}

// static
void URLLoaderFactoryManager::WillExecuteCode(content::RenderFrameHost* frame,
                                              const HostID& host_id) {
  if (host_id.type() != HostID::EXTENSIONS)
    return;

  const ExtensionRegistry* registry =
      ExtensionRegistry::Get(frame->GetProcess()->GetBrowserContext());
  DCHECK(registry);  // WillExecuteCode shouldn't happen during shutdown.
  const Extension* extension =
      registry->enabled_extensions().GetByID(host_id.id());
  DCHECK(extension);  // Guaranteed by the caller - see the doc comment.

  if (!IsSpecialURLLoaderFactoryRequired(*extension,
                                         FactoryUser::kContentScript))
    return;

  // When WillExecuteCode runs, the frame already received the initial
  // URLLoaderFactoryBundle - therefore we need to request a separate push
  // below.  This doesn't race with the ExtensionMsg_ExecuteCode message,
  // because the URLLoaderFactoryBundle is sent to the renderer over
  // content.mojom.FrameNavigationControl interface which is associated with the
  // legacy IPC pipe (raciness will be introduced if that ever changes).
  constexpr bool kPushToRendererNow = true;

  MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
      frame, {url::Origin::Create(extension->url())}, kPushToRendererNow);
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
URLLoaderFactoryManager::CreateFactory(
    content::RenderProcessHost* process,
    network::mojom::NetworkContext* network_context,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    const url::Origin& origin,
    const url::Origin& main_world_origin,
    const base::Optional<net::NetworkIsolationKey>& network_isolation_key) {
  content::BrowserContext* browser_context = process->GetBrowserContext();
  const ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
  DCHECK(registry);  // CreateFactory shouldn't happen during shutdown.

  // Opaque origins normally don't inherit security properties of their
  // precursor origins, but here opaque origins (e.g. think data: URIs) created
  // by an extension should inherit CORS/CORB treatment of the extension.
  url::SchemeHostPort precursor_origin =
      origin.GetTupleOrPrecursorTupleIfOpaque();

  // Don't create a factory for something that is not an extension.
  if (precursor_origin.scheme() != kExtensionScheme)
    return mojo::NullRemote();

  // Find the |extension| associated with |initiator_origin|.
  const Extension* extension =
      registry->enabled_extensions().GetByID(precursor_origin.host());
  if (!extension) {
    // This may happen if an extension gets disabled between the time
    // RenderFrameHost::MarkIsolatedWorldAsRequiringSeparateURLLoaderFactory is
    // called and the time
    // ContentBrowserClient::CreateURLLoaderFactory is called.
    return mojo::NullRemote();
  }

  // Figure out if the factory is needed for content scripts VS extension
  // renderer.
  FactoryUser factory_user = FactoryUser::kContentScript;
  ProcessMap* process_map = ProcessMap::Get(browser_context);
  if (process_map->Contains(extension->id(), process->GetID()))
    factory_user = FactoryUser::kExtensionProcess;

  // Create the factory (but only if really needed).
  if (!IsSpecialURLLoaderFactoryRequired(*extension, factory_user))
    return mojo::NullRemote();
  return CreateURLLoaderFactory(process, network_context, header_client,
                                *extension, main_world_origin,
                                network_isolation_key);
}

// static
void URLLoaderFactoryManager::AddExtensionToAllowlistForTesting(
    const Extension& extension) {
  GetExtensionsAllowlist().insert(extension.hashed_id().value());
}

// static
void URLLoaderFactoryManager::RemoveExtensionFromAllowlistForTesting(
    const Extension& extension) {
  GetExtensionsAllowlist().erase(extension.hashed_id().value());
}

}  // namespace extensions
